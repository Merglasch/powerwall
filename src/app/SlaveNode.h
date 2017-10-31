/**
 * @file   SlaveNode.h
 * @author Sebastian Maisch <sebastian.maisch@uni-ulm.de>
 * @date   2016.11.25
 *
 * @brief  Declaration of the ApplicationNodeImplementation for the slave node.
 */

#pragma once

#include "core/SlaveNodeHelper.h"

namespace viscom {

    /* Roomgame currently has no data exclusive to slave nodes */
    class SlaveNode final : public SlaveNodeInternal {
    public:

        explicit SlaveNode(ApplicationNodeInternal* appNode);
        virtual ~SlaveNode() override;
		void DecodeData() override;
		void UpdateSyncedInfo() override;

        void ConfirmCurrentState() const;

		void UpdateFrame(double, double) override;
        void Draw2D(FrameBuffer& fbo) override;
		virtual void PostDraw() override;
    private:
        int automatonTransitionNr_ = 0;
    };
}
