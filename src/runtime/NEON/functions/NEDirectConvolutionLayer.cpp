/*
 * Copyright (c) 2017 ARM Limited.
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "arm_compute/runtime/NEON/functions/NEDirectConvolutionLayer.h"

#include "arm_compute/core/PixelValue.h"
#include "arm_compute/core/Utils.h"
#include "arm_compute/core/Validate.h"
#include "arm_compute/runtime/NEON/NEScheduler.h"

#include <cmath>
#include <tuple>

using namespace arm_compute;

NEDirectConvolutionLayer::NEDirectConvolutionLayer()
    : _accumulate_bias_kernel(), _conv_kernel(), _input_border_handler(), _accumulator()
{
}

void NEDirectConvolutionLayer::configure(ITensor *input, const ITensor *weights, const ITensor *bias, ITensor *output, const PadStrideInfo &conv_info)
{
    ARM_COMPUTE_ERROR_ON_DATA_TYPE_CHANNEL_NOT_IN(output, 1, DataType::QS8, DataType::F32);

    // Free accumulator
    if(_accumulator.buffer() != nullptr)
    {
        _accumulator.allocator()->free();
    }

    // Allocate the intermediate accumulator tensor in case of fixed point input
    if(output->info()->data_type() == DataType::QS8)
    {
        _accumulator.allocator()->init(TensorInfo(output->info()->tensor_shape(), 1, DataType::QS16, output->info()->fixed_point_position()));
        _conv_kernel.configure(input, weights, &_accumulator, conv_info);
        _accumulate_bias_kernel.configure(&_accumulator, bias, output);
        _accumulator.allocator()->allocate();
    }
    else
    {
        _conv_kernel.configure(input, weights, output, conv_info);
        _accumulate_bias_kernel.configure(output, bias);
    }

    // Add zero padding XY
    _input_border_handler.configure(input, _conv_kernel.border_size(), BorderMode::CONSTANT, PixelValue(0));
}

void NEDirectConvolutionLayer::run()
{
    _input_border_handler.run(_input_border_handler.window());

    NEScheduler::get().schedule(&_conv_kernel, Window::DimZ);
    NEScheduler::get().schedule(&_accumulate_bias_kernel, Window::DimY);
}
