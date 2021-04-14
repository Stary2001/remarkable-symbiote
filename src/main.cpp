#include <getopt.h>

#include <cstddef>
#include <fstream>

#include <vector>
#include "rmkit.h"
#include "ShapeRecognizer.h"
#include "ShapeRecognizerResult.h"
#include "Stroke.h"

struct Action {
	bool is_shape;
	Stroke stroke;

	void draw_dashed_line(framebuffer::FB *fb, int x0, int y0, int x1, int y1, int width, remarkable_color color) {
		double len = sqrt(pow(x1-x0, 2) + pow(y1-y0, 2));

		int n = len/20;
		if(n == 0) {
			n = 1;
		}

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

	void draw(framebuffer::FB *fb) {
		if(is_shape) {
			// draw dotted
			if(stroke.getPointCount() < 10) {
				Point last_p = stroke.getPoint(0);
				for(int i = 1; i < stroke.getPointCount(); i++) {
					Point p = stroke.getPoint(i);
					draw_dashed_line(fb, last_p.x, last_p.y, p.x, p.y, 2, BLACK);
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

				Point last_p = stroke.getPoint(0);
				for(int i = 1; i < stroke.getPointCount(); i++) {
					Point p = stroke.getPoint(i);
					if(i%two_n < n) {
						fb->draw_line(last_p.x, last_p.y, p.x, p.y, 2, BLACK);
					}
					last_p = p;
				}
			}
		}
		else {
			Point last_p = stroke.getPoint(0);
			for(int i = 1; i < stroke.getPointCount(); i++) {
				Point p = stroke.getPoint(i);
				fb->draw_line(last_p.x, last_p.y, p.x, p.y, 2, BLACK);
				last_p = p;
			}
		}
	}
};

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

	void render() override {
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

class DrawingArea : public ui::Widget {
public:
	DrawingArea(int x, int y, int w, int h): ui::Widget(x, y, w, h) {}
	std::vector<Action> actions;

	void render() override {
		for(Action &a: actions) {
			a.draw(fb);
		}
	}
};

class App {
public:
	bool dump_strokes = false;
	FILE *dump_strokes_file = nullptr;

	bool last_touch = false;

	int last_x = 0;
	int last_y = 0;

	int ignore_height = 0;

	std::shared_ptr<framebuffer::FB> fb;

	Stroke current_stroke;
	std::vector<input_event> pending;

	DrawingArea *area;

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

	App(FILE *stroke_file) {
		dump_strokes = stroke_file != nullptr;
		dump_strokes_file = stroke_file;

		current_stroke = Stroke();

		fb = framebuffer::get();
		int w,h;
		std::tie(w,h) = fb->get_display_size();
		area = new DrawingArea(0,0,w,h);

		fb->dither = framebuffer::DITHER::BAYER_2;
		fb->waveform_mode = WAVEFORM_MODE_DU;

		auto scene = ui::make_scene();
		ui::MainLoop::set_scene(scene);
		app_bg = new AppBackground(0, 0, w, h);
		scene->add(app_bg);

		scene->add(area);

		auto style = ui::Stylesheet()
					 .valign(ui::Style::VALIGN::MIDDLE)
					 .justify(ui::Style::JUSTIFY::CENTER);

		auto large_style = style.font_size(48);

		auto h_layout_top = ui::HorizontalLayout(0, 15, w, 50, scene);
		auto h_layout_bot = ui::HorizontalLayout(w/8, h-60, w*0.75, 50, scene);

		ignore_height = h-60;

		auto running_text = new ui::Text(0, 0, 200, 50, "Shapes On");
		auto undo_button = new ui::Button(0, 0, 200, 50, "undo");
		auto done_button = new ui::Button(0, 0, 200, 50, "done");

		h_layout_top.pack_center(running_text);
		h_layout_bot.pack_start(undo_button);
		h_layout_bot.pack_end(done_button);

		running_text->set_style(large_style);
		undo_button->set_style(style);
		done_button->set_style(style);

		done_button->mouse.click += PLS_LAMBDA(auto &ev) {
			draw_strokes();
			exit(0);
		};

		undo_button->mouse.click += PLS_LAMBDA(auto &ev) {
			if(area->actions.size() > 0) {
				area->actions.pop_back();
				ui::MainLoop::full_refresh();
			}
		};
	}

	~App() {
		/*delete app_bg;
		delete area;*/
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

	const char *getShapeName(ShapeType type) {
		switch(type) {
			case ShapeType::Unknown:
				return "unk";
			break;
			case ShapeType::Line:
				return "line";
			break;
			case ShapeType::Triangle:
				return "triangle";
			break;
			case ShapeType::Rectangle:
				return "rectangle";
			break;
			case ShapeType::Circle:
				return "circle";
			break;
			case ShapeType::Arrow:
				return "arrow";
			break;
			case ShapeType::Quad:
				return "quad";
			break;
			default:
				return "uhh";
			break;
		}
	}

	void handle_motion_event(input::SynMotionEvent &event) {
		input::WacomEvent *wacom = input::is_wacom_event(event);
		if(wacom) {
			if(event.y > ignore_height) return;

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
					if(dump_strokes) {
						fprintf(dump_strokes_file, "stroke %i ", current_stroke.getPointCount());

						for(int i = 0; i < current_stroke.getPointCount(); i++) {
							Point p = current_stroke.getPoint(i);
							fprintf(dump_strokes_file, "%0.0f,%0.0f ", p.x, p.y);
						}

						fprintf(dump_strokes_file, "\n");
					}

					ShapeRecognizerResult *res = recognizer.recognizePatterns(&current_stroke);
					if(res) {
						Stroke *s = res->getRecognized();

						if(dump_strokes) {
							printf("type: %i, data: %p\n", res->type, res->data);
							if(res->type == ShapeType::Circle) {
								CircleData *circle = (CircleData*)res->data;
								fprintf(dump_strokes_file, "shape circle %0.2f %0.2f %0.2f", circle->center_x, circle->center_y, circle->radius);
							} else {
								fprintf(dump_strokes_file, "shape %s %i ", getShapeName(res->type), s->getPointCount());
								for(int i = 0; i < s->getPointCount()-1; i++) {
									Point p = s->getPoint(i);
									fprintf(dump_strokes_file, "%0.2f,%0.2f ", p.x, p.y);
								}
							}

							fprintf(dump_strokes_file, "\n");
						}
						
						// first, clear out the old line
						Point last_p = current_stroke.getPoint(0);
						for(int i=1; i<current_stroke.getPointCount();i++) {
							Point p = current_stroke.getPoint(i);
							fb->draw_line(last_p.x, last_p.y, p.x, p.y, 4, WHITE);
							last_p = p;
						}

						area->actions.push_back(Action {is_shape: true, stroke: *s});
						area->actions.back().draw(fb.get());
					} else {
						if(dump_strokes) {
							fprintf(dump_strokes_file, "shape none\n");
						}

						area->actions.push_back(Action{ is_shape: false, stroke: current_stroke });
					}
					printf("Done with stroke with %i points.\n", current_stroke.getPointCount());
					current_stroke = Stroke();
				}
			}

			last_x = event.x;
			last_y = event.y;
			last_touch = touch;
		} else {
			//done = true;
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

	void draw_strokes() {
		cleanup();
		start();
		// we're done, start drawing the shapes we recognized
		for(Action &a: area->actions) {
			if(a.is_shape) {
				pen_clear();
				
				//printf("drawing stroke with %i points\n", stroke.getPointCount());
				Point last_p = a.stroke.getPoint(0);
				pen_down(last_p.x, last_p.y, 2);
				for(int i = 1; i < a.stroke.getPointCount(); i++) {
					Point p = a.stroke.getPoint(i);
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
		}

		stop();
	}

	void run() {
		//ui::MainLoop::key_event += PLS_DELEGATE(self.handle_key_event)
		ui::MainLoop::motion_event += PLS_DELEGATE(handle_motion_event);

		ui::MainLoop::in.grab();
		ui::MainLoop::refresh();
		ui::MainLoop::redraw();
		while(true) {
			ui::MainLoop::main();
			ui::MainLoop::redraw();
			ui::MainLoop::read_input();
		}
	}
};

int main(int argc, char **argv) {
	int c;
	FILE *stroke_file = nullptr;

	static struct option long_options[] =
	{
		{"dump-strokes", required_argument, 0, 0},
		{0, 0, 0, 0}
	};

	while (1)
	{
		int option_index = 0;

		c = getopt_long (argc, argv, "",
					   long_options, &option_index);

		/* Detect the end of the options. */
		if (c == -1)
			break;

		switch (c)
		{
			case 0:
				/* If this option set a flag, do nothing else now. */
				if (long_options[option_index].flag != 0)
					break;

				if(strcmp(long_options[option_index].name, "dump-strokes") == 0 && optarg) {
					stroke_file = fopen(optarg, "w");
					if(stroke_file == nullptr) {
						printf("Failed to open %s (%s)\n", optarg, strerror(errno));
						return -1;
					}
				}
			break;

			case '?':
				/* getopt_long already printed an error message. */
				break;

			default:
				abort ();
		}
	}

	App app(stroke_file);
	app.run();
	if(stroke_file != nullptr) fclose(stroke_file);
}


// TODO TODO
// pen strokes that intersect button should not be counted