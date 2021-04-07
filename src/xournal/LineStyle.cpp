#include "LineStyle.h"

LineStyle::LineStyle() = default;

LineStyle::LineStyle(const LineStyle& other) { *this = other; }

LineStyle::~LineStyle() {
    free(this->dashes);
    this->dashes = nullptr;
    this->dashCount = 0;
}

void LineStyle::operator=(const LineStyle& other) {
    if (this == &other) {
        return;
    }
    const double* dashes = nullptr;
    int dashCount = 0;

    other.getDashes(dashes, dashCount);
    setDashes(dashes, dashCount);
}


/**
 * Get dash array and count
 *
 * @return true if dashed
 */
auto LineStyle::getDashes(const double*& dashes, int& dashCount) const -> bool {
    dashes = this->dashes;
    dashCount = this->dashCount;

    return this->dashCount > 0;
}

/**
 * Set the dash array and count
 *
 * @param dashes Dash data, will be copied
 * @param dashCount Count of entries
 */
// Todo(fabian): memmory use after free
void LineStyle::setDashes(const double* dashes, int dashCount) {
    free(this->dashes);
    if (dashCount == 0 || dashes == nullptr) {
        this->dashCount = 0;
        this->dashes = nullptr;
        return;
    }

    this->dashes = static_cast<double*>(malloc(dashCount * sizeof(double)));
    this->dashCount = dashCount;

    memcpy(this->dashes, dashes, this->dashCount * sizeof(double));
}

/**
 * Get dash array and count
 *
 * @return true if dashed
 */
auto LineStyle::hasDashes() const -> bool { return this->dashCount > 0; }
