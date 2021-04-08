/*
 * Xournal++
 *
 * Xournal Shape recognizer result
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <string>
#include <vector>

#include "XournalType.h"

class Stroke;

class ShapeRecognizer;

enum class ShapeType {
	Unknown,
	Line,
	Triangle,
	Rectangle,
	Circle,
	Arrow,
	Quad
};

struct ShapeData {};
struct CircleData : public ShapeData {
	CircleData(double x, double y, double rad): center_x(x), center_y(y), radius(rad) {}
	double center_x;
	double center_y;
	double radius;
};

class ShapeRecognizerResult {
public:
    ShapeRecognizerResult(Stroke* result);
    ShapeRecognizerResult(Stroke* result, ShapeType type, ShapeData *data);
    ShapeRecognizerResult(Stroke* result, ShapeRecognizer* recognizer);
    ShapeRecognizerResult(Stroke* result, ShapeRecognizer* recognizer, ShapeType type, ShapeData *data);

    virtual ~ShapeRecognizerResult();

public:
    void addSourceStroke(Stroke* s);
    Stroke* getRecognized();
    vector<Stroke*>* getSources();

    ShapeType type;
    ShapeData *data;
private:
    Stroke* recognized;
    vector<Stroke*> source;
};
