#include <cstddef>
#include <fstream>

#include <vector>
#include "rmkit.h"
#include "ShapeRecognizer.h"
#include "ShapeRecognizerResult.h"
#include "Stroke.h"

class AppBackground: public ui::Widget {
  public:
  int byte_size;
  framebuffer::VirtualFB *vfb = NULL;

  AppBackground(int x, int y, int w, int h): ui::Widget(x, y, w, h) {
    	byte_size = w*h*sizeof(remarkable_color);

    	int fw, fh;

    	std::tie(fw,fh) = fb->get_display_size();
    	vfb = new framebuffer::VirtualFB(fw, fh);
	    vfb->clear_screen();
    	vfb->fbmem = (remarkable_color*) memcpy(vfb->fbmem, fb->fbmem, byte_size);
	}

	void render(){
		if(rm2fb::IN_RM2FB_SHIM) {
    		fb->waveform_mode = WAVEFORM_MODE_GC16;
		}
    	else {
    		fb->waveform_mode = WAVEFORM_MODE_AUTO;
    	}

    	memcpy(fb->fbmem, vfb->fbmem, byte_size);

    	fb->perform_redraw(true);
    	fb->dirty = 1;
	}
};

class App {
public:
	bool done = false;
	bool last_touch = false;
	int last_x;
	int last_y;

	std::shared_ptr<framebuffer::FB> fb;
	std::vector<Stroke> strokes;
	std::vector<Stroke> shape_strokes;
	Stroke current_stroke;
	std::vector<input_event> pending;

	ShapeRecognizer recognizer;

	int touch_fd = -1;
	int pen_fd = -1;

	AppBackground *app_bg;

	int get_pen_x(double x) {
		return round(x / WACOM_X_SCALAR);
	}

	int get_pen_y(double y){
	  return round(WACOMHEIGHT - (y / WACOM_Y_SCALAR));
	}

	App() {
		current_stroke = Stroke();

		fb = framebuffer::get();
		int w,h;
		std::tie(w,h) = fb->get_display_size();

	    //fb->dither = framebuffer::DITHER::BAYER_2;
	    //fb->waveform_mode = WAVEFORM_MODE_DU;

	    auto scene = ui::make_scene();
	    ui::MainLoop::set_scene(scene);
	    app_bg = new AppBackground(0, 0, w, h);
	    scene->add(app_bg);
	}

	void draw_dashed_line(int x0, int y0, int x1, int y1, int width, remarkable_color color) {
		double len = sqrt(pow(x1-x0, 2) + pow(y1-y0, 2));

		int n = len/20;

		double x = x0;
		double y = y0;
		double dx = (x1-x0)/n;
		double dy = (y1-y0)/n;

		for(int j = 0; j < n; j++) {
			if(j%2 == 0) {
				fb->draw_line(x, y, x+dx, y+dy, width, color);
			}
			x += dx;
			y += dy;
		}
	}

	void pen_clear() {
		pending.push_back(input_event{ type:EV_ABS, code:ABS_X, value: -1 });
		pending.push_back(input_event{ type:EV_ABS, code:ABS_DISTANCE, value: -1 });
		pending.push_back(input_event{ type:EV_ABS, code:ABS_PRESSURE, value: -1});
		pending.push_back(input_event{ type:EV_ABS, code:ABS_Y, value: -1 });
		pending.push_back(input_event{ type:EV_SYN, code:SYN_REPORT, value:1 });
		flush_strokes();
	}

	void pen_line(double x0, double y0, double x1, double y1, int points=10) {
		pen_down(x0, y0);
		pen_move(x0, y0, x1, y1, points);
		pen_up();
	}

	int moves = 0;
	void pen_move(double x0, double y0, double x1, double y1, int points=10) {
		double dx = double(x1-x0) / double(points);
  		double dy = double(y1-y0) / double(points);

		pending.push_back(input_event{ type:EV_SYN, code:SYN_REPORT, value:1 });
	  	for(int i = 0; i <= points; i++) {
	    	pending.push_back(input_event{ type:EV_ABS, code:ABS_Y, value: get_pen_x(x0 + (i*dx)) });
	    	pending.push_back(input_event{ type:EV_ABS, code:ABS_X, value: get_pen_y(y0 + (i*dy)) });
	    	pending.push_back(input_event{ type:EV_SYN, code:SYN_REPORT, value:1 });
	    }

	    moves++;
	    if(moves > 100) {
			flush_strokes(10);
		}
	}


	void pen_down(int x, int y, int points=10) {
		pending.push_back(input_event{ type:EV_KEY, code:BTN_TOOL_PEN, value: 1 });
		pending.push_back(input_event{ type:EV_KEY, code:BTN_TOUCH, value: 1 });
		pending.push_back(input_event{ type:EV_ABS, code:ABS_Y, value: get_pen_x(x) });
		pending.push_back(input_event{ type:EV_ABS, code:ABS_X, value: get_pen_y(y) });
		pending.push_back(input_event{ type:EV_ABS, code:ABS_DISTANCE, value: 0 });
		pending.push_back(input_event{ type:EV_ABS, code:ABS_PRESSURE, value: 4000 });
		pending.push_back(input_event{ type:EV_SYN, code:SYN_REPORT, value:1 });

		for(int i = 0; i < points; i++) {
    		pending.push_back(input_event{ type:EV_ABS, code:ABS_PRESSURE, value: 4000 });
    		pending.push_back(input_event{ type:EV_ABS, code:ABS_PRESSURE, value: 4001 });
		    pending.push_back(input_event{ type:EV_SYN, code:SYN_REPORT, value:1 });
		}
		flush_strokes(10);
	}

	void pen_up() {
		pending.push_back(input_event{ type:EV_KEY, code:BTN_TOOL_PEN, value: 0 });
		pending.push_back(input_event{ type:EV_KEY, code:BTN_TOUCH, value: 0 });
		pending.push_back(input_event{ type:EV_SYN, code:SYN_REPORT, value:1 });

		flush_strokes();
	}

	void flush_strokes(int sleep_time=1000) {
		std::vector<input_event> send;
		for(auto event: pending){
			send.push_back(event);
		    if(event.type == EV_SYN) {
		    	if(sleep_time) {
		    		usleep(sleep_time);
		    	}
		      
		    	input_event *out = (input_event*) malloc(sizeof(input_event) * send.size());
		    	for(size_t i = 0; i < send.size(); i++) {
			        out[i] = send[i];
			    }

			    write(pen_fd, out, sizeof(input_event) * send.size());
		    	send.clear();
		    	free(out);
		    }
		}

		if(send.size() > 0) {
			printf("We have events left over!!\n");
		}

		pending.clear();
	}

	void handle_motion_event(input::SynMotionEvent &event) {
		input::WacomEvent *wacom = input::is_wacom_event(event);
		if(wacom) {
			bool touch = wacom->btn_touch == 1;
			if(touch == last_touch) {
				if(touch) {
					fb->draw_line(last_x, last_y, event.x, event.y, 2, BLACK);

					Point p(event.x, event.y);
					current_stroke.addPoint(p);
				}
			}
			else {
				if(!touch) {
					ShapeRecognizerResult *res = recognizer.recognizePatterns(&current_stroke);
					if(res) {
						// first, clear out the old line
						Point last_p = current_stroke.getPoint(0);
						for(int i=1; i<current_stroke.getPointCount();i++) {
							Point p = current_stroke.getPoint(i);
							fb->draw_line(last_p.x, last_p.y, p.x, p.y, 4, WHITE);
							last_p = p;
						}

						// now draw dotted
						Stroke *s = res->getRecognized();
						if(s->getPointCount() < 10) {
							last_p = s->getPoint(0);
							for(int i = 1; i < s->getPointCount(); i++) {
								Point p = s->getPoint(i);
								draw_dashed_line(last_p.x, last_p.y, p.x, p.y, 2, BLACK);
								last_p = p;
							}
						} 
						else {
							// draw a dashed circle
							// lots of segments -> lots of _small_ segments

							// TODO: calculate n from stroke length, this is a bit harder than for straight lines
							// maybe have recognizer dump circle info?
							int two_n = 6;
							int n = 3;

							last_p = s->getPoint(0);
							for(int i = 1; i < s->getPointCount(); i++) {
								Point p = s->getPoint(i);
								if(i%two_n < n) {
									fb->draw_line(last_p.x, last_p.y, p.x, p.y, 2, BLACK);
								}
								last_p = p;
							}
						}

						shape_strokes.push_back(*s);
					} else {
						strokes.push_back(current_stroke);
					}
					printf("Done with stroke with %i points.\n", current_stroke.getPointCount());
					current_stroke = Stroke();
				}
			}

			last_x = event.x;
			last_y = event.y;
			last_touch = touch;
		} else {
			done = true;
		}
	}

	int fd0;
	int fd1;
	int fd2;

	void start() {
		fd0 = open("/dev/input/event0", O_RDWR);
		fd1 = open("/dev/input/event1", O_RDWR);
		fd2 = open("/dev/input/event2", O_RDWR);

		if(input::id_by_capabilities(fd0) == input::EV_TYPE::TOUCH)
			touch_fd = fd0;
		if(input::id_by_capabilities(fd1) == input::EV_TYPE::TOUCH)
			touch_fd = fd1;
		if(input::id_by_capabilities(fd2) == input::EV_TYPE::TOUCH)
			touch_fd = fd2;

		if(input::id_by_capabilities(fd0) == input::EV_TYPE::STYLUS)
			pen_fd = fd0;
		if(input::id_by_capabilities(fd1) == input::EV_TYPE::STYLUS)
			pen_fd = fd1;
		if(input::id_by_capabilities(fd2) == input::EV_TYPE::STYLUS)
			pen_fd = fd2;
	}

	void stop() {
		close(fd0);
		close(fd1);
		close(fd2);
	}

	void cleanup() {
		app_bg->render();
		ui::MainLoop::in.ungrab();
	}

	void run() {
		//ui::MainLoop::key_event += PLS_DELEGATE(self.handle_key_event)
		ui::MainLoop::motion_event += PLS_DELEGATE(handle_motion_event);

	    ui::MainLoop::in.grab();
		ui::MainLoop::refresh();
		ui::MainLoop::redraw();
		while(!done) {
			ui::MainLoop::main();
			ui::MainLoop::redraw();
			ui::MainLoop::read_input();
		}

		cleanup();
		start();

		// we're done, start drawing the shapes we recognized
		for(Stroke &stroke: shape_strokes) {
			pen_clear();
			
			//printf("drawing stroke with %i points\n", stroke.getPointCount());
			Point last_p = stroke.getPoint(0);
			pen_down(last_p.x, last_p.y, 2);
			for(int i = 1; i < stroke.getPointCount(); i++) {
				Point p = stroke.getPoint(i);
				double len = sqrt(pow(p.x-last_p.x, 2) + pow(p.y-last_p.y, 2));
				// keep each segment roughly the same length
				// TODO: maybe change this to fall off at larger lengths later
				int n_points = (int)len;
				pen_move(last_p.x, last_p.y, p.x, p.y, n_points);

				last_p = p;
			}
			flush_strokes(10);

			pen_up();
		}

		stop();
	}
};

int main(int argc, char **argv) {
	(void) argc;
	(void) argv;

	App app;
	app.run();
}