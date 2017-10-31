#include "DragAndZoomCamera.h"

DragAndZoomCamera::DragAndZoomCamera(glm::mat4 view_projection) {
	view_projection_ = view_projection;
	isTouched_ = false;
	last_position_ = glm::vec2(0);
	drag_speed_ = 0.8f;
	zoom_speed_ = 0.4f;
}

void DragAndZoomCamera::onTouch() {
	isTouched_ = true;
}

void DragAndZoomCamera::onRelease() {
	isTouched_ = false;
}

void DragAndZoomCamera::onMouseMove(float newx, float newy) {
	if (isTouched_) {
		float deltax = newx - last_position_.x;
		float deltay = newy - last_position_.y;
		view_projection_[3][0] += (deltax * drag_speed_);
		view_projection_[3][1] += (-deltay * drag_speed_);
	}
	last_position_.x = newx;
	last_position_.y = newy;
}

void DragAndZoomCamera::onScroll(float offset) {
	view_projection_[3][2] += (offset * zoom_speed_);
}

void DragAndZoomCamera::setXRotation(float angle) {
	view_projection_ = glm::rotate(view_projection_, angle, glm::vec3(1, 0, 0));
}