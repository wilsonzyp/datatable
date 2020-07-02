//------------------------------------------------------------------------------
// Copyright 2019-2020 H2O.ai
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//------------------------------------------------------------------------------
#include "parallel/api.h"           // this_thread_index
#include "parallel/thread_job.h"    // ThreadJob
namespace dt {


thread_task::~thread_task() {}


ThreadJob::~ThreadJob() {}


void ThreadJob::abort_execution() {
  // default implementation does nothing
}


void ThreadJob::execute_in_current_thread() {
  // If this throws an exception, it will propagate to outer level, where the
  // exception will get caught in the outer's level task executor.
  size_t ith = dt::this_thread_index();
  while (true) {
    auto task = get_next_task(ith);
    if (!task) break;
    task->execute(nullptr);
  }
}



}  // namespace dt
