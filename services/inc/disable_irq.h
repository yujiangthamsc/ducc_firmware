/*
 * Copyright (c) 2016 Particle Industries, Inc.  All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SERVICES_DISABLE_IRQ_H
#define SERVICES_DISABLE_IRQ_H

#include "interrupts_hal.h"

// Scope guard for HAL_disable_irq()
class DisableIRQ {
public:
    DisableIRQ() :
            mask_(HAL_disable_irq()),
            active_(true) {
    }

    ~DisableIRQ() {
        dismiss();
    }

    void dismiss() {
        if (active_) {
            HAL_enable_irq(mask_);
            active_ = false;
        }
    }

private:
    int mask_;
    bool active_;
};

#endif // SERVICES_DISABLE_IRQ_H
