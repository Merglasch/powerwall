#include "AutomatonUpdater.h"
#include "app/roomgame/GridCell.h"
#include "GPUCellularAutomaton.h"
#include "core/resources/GPUProgramManager.h"
#include "InteractiveGrid.h"

namespace roomgame
{
    GPUCellularAutomaton::GPUCellularAutomaton(AutomatonUpdater* automatonGrid_grid, std::shared_ptr<InteractiveGrid> interactiveGrid, double transition_time) :
        automatonUpdater_(automatonGrid_grid),
        interactiveGrid_(interactiveGrid),
        transition_time_(transition_time),
        last_time_(0.0),
        delta_time_(0.0),
        is_initialized_(false),
        current_read_index_(0),
        sizeof_tmp_client_buffer_(0),
        tmp_client_buffer_(0),
        framebuffer_pair_{ 0, 0 }
    {
        pixel_size_ = glm::vec2(1.0f / float(interactiveGrid_->getNumColumns()), 1.0f / float(interactiveGrid_->getNumRows()));
        DEFAULT_TRANSITION_TIME = transition_time;
        automatonUpdater_->setCellularAutomaton(this);
    }

    void GPUCellularAutomaton::cleanup() {
        if (is_initialized_) {
            glDeleteTextures(1, &texture_pair_[0].id);
            glDeleteTextures(1, &texture_pair_[1].id);
            delete framebuffer_pair_[0];
            delete framebuffer_pair_[1];
            free(tmp_client_buffer_);
        }
    }

    void GPUCellularAutomaton::init(viscom::GPUProgramManager mgr) {
        if (is_initialized_) return;
        // Shader
        shader_ = mgr.GetResource("cellularAutomaton",
            std::initializer_list<std::string>{ "cellularAutomaton.vert", "cellularAutomaton.frag" });
        pixel_size_uniform_location_ = shader_->getUniformLocation("pxsize");
        texture_uniform_location_ = shader_->getUniformLocation("inputGrid");
        // Two framebuffers with textures
        GLuint cols = (GLuint)interactiveGrid_->getNumColumns();
        GLuint rows = (GLuint)interactiveGrid_->getNumRows();
        texture_pair_[0].attachmentType = texture_pair_[1].attachmentType = GL_COLOR_ATTACHMENT0;
        texture_pair_[0].sized_format = texture_pair_[1].sized_format = roomgame::GRID_STATE_TEXTURE.sized_format;
        texture_pair_[0].format = texture_pair_[1].format = roomgame::GRID_STATE_TEXTURE.format;
        texture_pair_[0].datatype = texture_pair_[1].datatype = roomgame::GRID_STATE_TEXTURE.datatype;
        framebuffer_pair_[0] = new GPUBuffer(cols, rows, { &texture_pair_[0] });
        framebuffer_pair_[1] = new GPUBuffer(cols, rows, { &texture_pair_[1] });
        // Allocate temporary client buffer to transfer pixels from and to
        sizeof_tmp_client_buffer_ = cols * rows *
            roomgame::GRID_STATE_TEXTURE_CHANNELS * sizeof(roomgame::GRID_STATE_ELEMENT);
        tmp_client_buffer_ = (roomgame::GRID_STATE_ELEMENT*)malloc(sizeof_tmp_client_buffer_);
        for (size_t i = 0; i < cols * rows * roomgame::GRID_STATE_TEXTURE_CHANNELS; i++)
            tmp_client_buffer_[i] = 0;
        if (!tmp_client_buffer_) throw std::runtime_error("");
        // Get initial state of grid
        copyFromGridToTexture(0);
        // Screen filling quad
        glGenVertexArrays(1, &vao_);
        glBindVertexArray(vao_);
        GLfloat quad[] = {
            // (x, y)      // (u, v)
            -1.0f,  1.0f,  0.0f, 1.0f, // top left
            -1.0f, -1.0f,  0.0f, 0.0f, // bottom left
            1.0f, -1.0f,  1.0f, 0.0f, // bottom right
            -1.0f,  1.0f,  0.0f, 1.0f, // top left
            1.0f, -1.0f,  1.0f, 0.0f, // bottom right
            1.0f,  1.0f,  1.0f, 1.0f // top right
        };
        GLuint vbo;
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (GLvoid*)0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (GLvoid*)(2 * sizeof(GLfloat)));
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glBindVertexArray(0);
        is_initialized_ = true;
    }

    void GPUCellularAutomaton::copyFromGridToTexture(int pair_index) {
        size_t rows = interactiveGrid_->getNumColumns();
        size_t cols = interactiveGrid_->getNumRows();
        const unsigned int N_CH = roomgame::GRID_STATE_TEXTURE_CHANNELS;
        for (unsigned int x = 0; x < cols; x++) {
            for (unsigned int y = 0; y < rows * N_CH - 1; y += N_CH) {
                GridCell* c = interactiveGrid_->getCellAt(y / N_CH, x);
                tmp_client_buffer_[x * N_CH * rows + y] = (roomgame::GRID_STATE_ELEMENT) c->getBuildState();
                tmp_client_buffer_[x * N_CH * rows + y + 1] = (roomgame::GRID_STATE_ELEMENT) c->getHealthPoints();
            }
        }
        glBindTexture(GL_TEXTURE_2D, texture_pair_[pair_index].id);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, (GLsizei)cols, (GLsizei)rows,
            texture_pair_[pair_index].format, texture_pair_[pair_index].datatype, tmp_client_buffer_);
    }

    void GPUCellularAutomaton::copyFromTextureToGrid(int pair_index) {
        size_t rows = interactiveGrid_->getNumColumns();
        size_t cols = interactiveGrid_->getNumRows();
        const unsigned int N_CH = roomgame::GRID_STATE_TEXTURE_CHANNELS;
        // download texture
        glBindTexture(GL_TEXTURE_2D, texture_pair_[pair_index].id);
        glGetTexImage(GL_TEXTURE_2D, 0, texture_pair_[pair_index].format,
            texture_pair_[pair_index].datatype, tmp_client_buffer_);
        // iterate over contents
        for (unsigned int x = 0; x < cols; x++) {
            for (unsigned int y = 0; y < rows * N_CH - 1; y += N_CH) {
                GLuint state = static_cast<GLuint>(tmp_client_buffer_[x * N_CH * rows + y]);
                int hp = static_cast<int>(tmp_client_buffer_[x * N_CH * rows + y + 1]);
                GridCell* c = interactiveGrid_->getCellAt(y / N_CH, x);
                // something changed?
                if (c->getBuildState() == state && c->getHealthPoints() == hp)
                    continue;
                // then update CPU side
                automatonUpdater_->updateGridAt(c, state, hp);
            }
        }
    }

    void GPUCellularAutomaton::updateCell(GridCell* c, GLuint buildState, GLuint hp) {
        if (!is_initialized_) return;
        // Upload possibly new build state, health and "is infected?"-UNORM
        roomgame::GRID_STATE_ELEMENT data[roomgame::GRID_STATE_TEXTURE_CHANNELS] = {
            buildState,
            hp,
            (buildState & GridCell::INFECTED) ? 0xFFFFFFFFU : 0,
            static_cast<roomgame::GRID_STATE_ELEMENT>(static_cast<float>(hp) / static_cast<float>(GridCell::MAX_HEALTH) * 0xFFFFFFFFU) 
        };
        glBindTexture(GL_TEXTURE_2D, texture_pair_[current_read_index_].id);
        glTexSubImage2D(GL_TEXTURE_2D, 0, (GLint)c->getCol(), (GLint)c->getRow(), 1, 1,
            texture_pair_[0].format, texture_pair_[0].datatype, data);
    }

    bool GPUCellularAutomaton::checkForTransitionTexSwapWithDeltaReset(double time, bool oldVal)
    {
        delta_time_ = time - last_time_;
        if (delta_time_ >= transition_time_) {
            last_time_ = time;
            delta_time_ = 0;
            return !oldVal;
        }
        return oldVal;
    }

    void GPUCellularAutomaton::transition() {
        // Test if it is time for the next generation

        int current_write_index = (current_read_index_ == 0) ? 1 : 0;
        // Do transition on gpu
        framebuffer_pair_[current_write_index]->bind();
        glViewport(0, 0, (GLsizei)interactiveGrid_->getNumColumns(), (GLsizei)interactiveGrid_->getNumRows());
        glDisable(GL_DEPTH_TEST);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(shader_->getProgramId());
        glBindVertexArray(vao_);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture_pair_[current_read_index_].id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); // repeat makes a torus-shaped playing field
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glUniform1i(texture_uniform_location_, 0);
        glUniform2f(pixel_size_uniform_location_, pixel_size_.x, pixel_size_.y);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        glEnable(GL_DEPTH_TEST);
        // Update grid
        automatonUpdater_->onTransition();
        copyFromTextureToGrid(current_write_index); // Performance bottleneck
                                                    // Swap buffers
        current_read_index_ = current_write_index;
    }

    void GPUCellularAutomaton::setTransitionTime(double t) {
        transition_time_ = t;
    }

    GLfloat GPUCellularAutomaton::getTimeDeltaNormalized() {
        return (GLfloat)(delta_time_ / transition_time_);
    }

    GLuint GPUCellularAutomaton::getLatestTexture() {
        return texture_pair_[current_read_index_].id;
    }

    GLuint GPUCellularAutomaton::getPreviousTexture() {
        return texture_pair_[(current_read_index_ + 1) % 2].id;
    }

    bool GPUCellularAutomaton::isInitialized() {
        return is_initialized_;
    }

    size_t GPUCellularAutomaton::getGridBufferSize() {
        return sizeof_tmp_client_buffer_;
    }

    size_t GPUCellularAutomaton::getGridBufferElements() {
        return sizeof_tmp_client_buffer_ / sizeof(roomgame::GRID_STATE_ELEMENT);
    }

    roomgame::GRID_STATE_ELEMENT* GPUCellularAutomaton::getGridBuffer() {
        return tmp_client_buffer_;
    }

    double GPUCellularAutomaton::getTransitionTime()
    {
        return transition_time_;
    }

    void GPUCellularAutomaton::ResetTransitionTime()
    {
        transition_time_ = DEFAULT_TRANSITION_TIME;
    }

}

