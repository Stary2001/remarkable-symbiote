#include "ShapeRecognizerResult.h"

#include "ShapeRecognizer.h"
//#include "Stacktrace.h"

ShapeRecognizerResult::ShapeRecognizerResult(Stroke* result) { this->recognized = result; this->type=ShapeType::Unknown; this->data=nullptr; }

ShapeRecognizerResult::ShapeRecognizerResult(Stroke* result, ShapeType type, ShapeData *data)
{
    this->recognized = result;
    this->type = type;
    this->data = data;
}

ShapeRecognizerResult::ShapeRecognizerResult(Stroke* result, ShapeRecognizer* recognizer) {
    this->recognized = result;

    for (int i = 0; i < recognizer->queueLength; i++) {
        if (recognizer->queue[i].stroke) {
            this->addSourceStroke(recognizer->queue[i].stroke);
        }
    }

    RDEBUG("source list length: %i", (int)this->source.size());

    this->type=ShapeType::Unknown; this->data=nullptr;
}

ShapeRecognizerResult::ShapeRecognizerResult(Stroke* result, ShapeRecognizer* recognizer, ShapeType type, ShapeData *data) {
    this->recognized = result;

    for (int i = 0; i < recognizer->queueLength; i++) {
        if (recognizer->queue[i].stroke) {
            this->addSourceStroke(recognizer->queue[i].stroke);
        }
    }

    RDEBUG("source list length: %i", (int)this->source.size());

    this->type = type;
    this->data = data;
}


ShapeRecognizerResult::~ShapeRecognizerResult() { this->recognized = nullptr; }

void ShapeRecognizerResult::addSourceStroke(Stroke* s) {
    for (Stroke* elem: this->source) {
        if (s == elem) {
            // this is a bug in the ShapreRecognizer
            // Ignore
            return;
        }
    }


    this->source.push_back(s);
}

auto ShapeRecognizerResult::getRecognized() -> Stroke* { return this->recognized; }

auto ShapeRecognizerResult::getSources() -> vector<Stroke*>* { return &this->source; }
