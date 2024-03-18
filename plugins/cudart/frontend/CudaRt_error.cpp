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

#include "CudaRt.h"

using namespace std;

extern "C" __host__ const char* CUDARTAPI
cudaGetErrorString(cudaError_t error) {
  CudaRtFrontend::Prepare();
  CudaRtFrontend::AddVariableForArguments(error);
  CudaRtFrontend::Execute("cudaGetErrorString");
#ifdef _WIN32
  char* error_string = _strdup(CudaRtFrontend::GetOutputString());
#else
  char* error_string = strdup(CudaRtFrontend::GetOutputString());
#endif
  return error_string;
}
extern "C" __host__ cudaError_t CUDARTAPI cudaPeekAtLastError(void) {
  CudaRtFrontend::Prepare();
  CudaRtFrontend::Execute("cudaPeekAtLastError");
  return CudaRtFrontend::GetExitCode();
}

extern "C" __host__ cudaError_t CUDARTAPI cudaGetLastError(void) {
  CudaRtFrontend::Prepare();
  CudaRtFrontend::Execute("cudaGetLastError");
  return CudaRtFrontend::GetExitCode();
}

extern "C" __host__ const char* CUDARTAPI cudaGetErrorName(cudaError_t error) {
    CudaRtFrontend::Prepare();
    CudaRtFrontend::AddVariableForArguments(error);
    CudaRtFrontend::Execute("cudaGetErrorName");
#ifdef _WIN32
    char* error_name = _strdup(CudaRtFrontend::GetOutputString());
#else
    char* error_name = strdup(CudaRtFrontend::GetOutputString());
#endif
    return error_name;
}