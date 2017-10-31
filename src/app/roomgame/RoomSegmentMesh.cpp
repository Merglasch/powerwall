#include "RoomSegmentMesh.h"

RoomSegmentMesh::RoomSegmentMesh(viscom::Mesh* mesh, viscom::GPUProgram* program, size_t pool_allocation_bytes) :
    SynchronizedInstancedMesh(mesh, program, pool_allocation_bytes),
	//room_ordered_buffer_(pool_allocation_bytes),
	next_free_offset_(0), last_free_offset_(0)
{
	// Connect instance buffers
	glBindVertexArray(vao_);
	//glBindBuffer(GL_ARRAY_BUFFER, room_ordered_buffer_.id_);
	//Instance::setAttribPointer();
	glBindBuffer(GL_ARRAY_BUFFER, gpu_instance_buffer_.id_);
	Instance::setAttribPointer();
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

RoomSegmentMesh::~RoomSegmentMesh() {
	//glDeleteBuffers(1, &room_ordered_buffer_.id_);
	while (next_free_offset_) {
		NextFreeOffsetQueueElem* tmp = next_free_offset_->el_;
		delete next_free_offset_;
		next_free_offset_ = tmp;
	}
}

RoomSegmentMesh::InstanceBufferRange RoomSegmentMesh::addInstanceUnordered(Instance i) {
    int offset; // the buffer offset where instance should be added
    // if there are no holes in the instance buffer, add instance at the end
    if (!next_free_offset_) offset = gpu_instance_buffer_.num_instances_;
    else { // else get hole offset and remove hole from queue
        offset = next_free_offset_->offset_;
        NextFreeOffsetQueueElem* tmp = next_free_offset_->el_;
        delete next_free_offset_;
        next_free_offset_ = tmp;
    }
    // insert instance into a hole or to the end of the buffer
    // data remains on CPU side
    // upload to GPU is deferred until SGCT does a synch (see base class SynchronizedInstancedMesh)
    if (offset >= instance_buffer_.size()) {
        instance_buffer_.push_back(i);
    }
    else {
        instance_buffer_[offset] = i;
    }
    RoomSegmentMesh::InstanceBufferRange range;
    range.buffer_ = &gpu_instance_buffer_;
    range.mesh_ = this;
    range.num_instances_ = 1;
    range.offset_instances_ = offset;
    if (offset == gpu_instance_buffer_.num_instances_)
        gpu_instance_buffer_.num_instances_++;
    return range; // return buffer range so that the caller can remove the instance again later
}

RoomSegmentMesh::InstanceBufferRange RoomSegmentMesh::addInstanceUnordered_IMMEDIATE_GPU_UPLOAD(Instance i) {
	int offset; // the buffer offset where instance should be added
    // if there are no holes in the instance buffer, add instance at the end
	if (!next_free_offset_) offset = gpu_instance_buffer_.num_instances_;
	else { // else get hole offset and remove hole from queue
        offset = next_free_offset_->offset_;
		NextFreeOffsetQueueElem* tmp = next_free_offset_->el_;
		delete next_free_offset_;
		next_free_offset_ = tmp;
	}
    // instance buffer reached current capacity?
	if ((offset + 1) * sizeof(Instance) > gpu_instance_buffer_.pool_allocation_bytes_ * gpu_instance_buffer_.num_reallocations_) {
		// allocate new buffer and and copy data
		glBindBuffer(GL_COPY_READ_BUFFER, gpu_instance_buffer_.id_);
		GLuint tmpBuffer;
		glGenBuffers(1, &tmpBuffer);
		glBindBuffer(GL_COPY_WRITE_BUFFER, tmpBuffer);
		gpu_instance_buffer_.num_reallocations_++;
		glBufferData(GL_COPY_WRITE_BUFFER, gpu_instance_buffer_.pool_allocation_bytes_ * gpu_instance_buffer_.num_reallocations_, 0, GL_STATIC_COPY);
		glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, gpu_instance_buffer_.num_instances_ * sizeof(Instance));
		// THIS IS WHAT MAKES REALLOCATION APPROACH ***UGLY***:
		glDeleteBuffers(1, &gpu_instance_buffer_.id_); // Delete old instance buffer
		glDeleteVertexArrays(1, &vao_); // Delete old VAO
		resetShader(); // Create new VAO and connect old vertex buffer
		// Connect new instance buffer
		gpu_instance_buffer_.id_ = tmpBuffer;
		glBindVertexArray(vao_);
		glBindBuffer(GL_ARRAY_BUFFER, gpu_instance_buffer_.id_);
		Instance::setAttribPointer();
	}
    // insert instance into a hole or to the end of the buffer
	glBindBuffer(GL_ARRAY_BUFFER, gpu_instance_buffer_.id_);
	glBufferSubData(GL_ARRAY_BUFFER, offset * sizeof(Instance), sizeof(Instance), &i);
    RoomSegmentMesh::InstanceBufferRange range;
    range.buffer_ = &gpu_instance_buffer_;
    range.mesh_ = this;
    range.num_instances_ = 1;
    range.offset_instances_ = offset;
	if (offset == gpu_instance_buffer_.num_instances_)
		gpu_instance_buffer_.num_instances_++;
	return range; // return buffer range so that the caller can remove the instance again later
}


void RoomSegmentMesh::removeInstanceUnordered(int offset_instances) {
    // removing last offset in buffer is done by simply decrementing instance count
	if (offset_instances == gpu_instance_buffer_.num_instances_ - 1) {
		gpu_instance_buffer_.num_instances_--;
		return;
	}
    // removing instance from the middle of the buffer creates a hole
	if (!next_free_offset_) { // create the first hole
		next_free_offset_ = new NextFreeOffsetQueueElem(offset_instances);
		last_free_offset_ = next_free_offset_;
	}
	else { // append a hole to the queue
		last_free_offset_->el_ = new NextFreeOffsetQueueElem(offset_instances);
		last_free_offset_ = last_free_offset_->el_;
	}
	// cannot prevent GL from rendering holes too: insert a zero scaled instance into the hole (bad hack)
	Instance zeroScaledInstance;
	/*glBindBuffer(GL_ARRAY_BUFFER, gpu_instance_buffer_.id_); // no more immediate GPU upload
	glBufferSubData(GL_ARRAY_BUFFER, offset_instances * sizeof(Instance), sizeof(Instance), &zeroScaledInstance);*/
    instance_buffer_[offset_instances] = zeroScaledInstance;
}

RoomSegmentMesh::InstanceBufferRange RoomSegmentMesh::moveInstancesToRoomOrderedBuffer(std::initializer_list<int> offsets) {
	//TODO copy and remove instances at given offsets
	return RoomSegmentMesh::InstanceBufferRange();
}



void RoomSegmentMesh::renderAllInstances(std::function<void(void)> uniformSetter, const glm::mat4& view_projection, GLint isDebugMode, LightInfo* lightInfo, glm::vec3& viewPos) {
	//TODO Seperately draw unordered and ordered buffers
	if (gpu_instance_buffer_.num_instances_ == 0) return;
	MeshBase::render(view_projection, gpu_instance_buffer_.num_instances_,uniformSetter,glm::mat4(1),false,lightInfo,viewPos, isDebugMode);
}