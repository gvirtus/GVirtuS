/*
 * gVirtuS -- A GPGPU transparent virtualization component.
 *
 * Copyright (C) 2009-2010  The University of Napoli Parthenope at Naples.
 *
 * This file is part of gVirtuS.
 *
 * gVirtuS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * gVirtuS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with gVirtuS; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Written by: Giuseppe Coviello <giuseppe.coviello@uniparthenope.it>,
 *             Department of Applied Science
 */

#include "CudaRtHandler.h"

CUDA_ROUTINE_HANDLER(GetErrorString) {
  /* const char* cudaGetErrorString(cudaError_t error) */

  try {
    cudaError_t error = input_buffer->Get<cudaError_t>();
    const char *error_string = cudaGetErrorString(error);
    std::shared_ptr<Buffer> output_buffer = std::make_shared<Buffer>();

    output_buffer->AddString(error_string);
    return std::make_shared<Result>(cudaSuccess, output_buffer);
  } catch (string e) {
    cerr << e << endl;
    return std::make_shared<Result>(cudaErrorMemoryAllocation);
  }
}

CUDA_ROUTINE_HANDLER(GetErrorName) {
    try {
        cudaError_t error = input_buffer->Get<cudaError_t>();
        const char *error_string = cudaGetErrorName(error);
        std::shared_ptr<Buffer> output_buffer = std::make_shared<Buffer>();

        output_buffer->AddString(error_string);
        return std::make_shared<Result>(cudaSuccess, output_buffer);
    } catch (string e) {
        cerr << e << endl;
        return std::make_shared<Result>(cudaErrorMemoryAllocation);
    }
}

CUDA_ROUTINE_HANDLER(PeekAtLastError) {
  /* cudaError_t  cudaPeekAtLastError(void) */
  return std::make_shared<Result>(cudaPeekAtLastError());
}

CUDA_ROUTINE_HANDLER(GetLastError) {
  /* cudaError_t cudaGetLastError(void) */
  return std::make_shared<Result>(cudaGetLastError());
}
