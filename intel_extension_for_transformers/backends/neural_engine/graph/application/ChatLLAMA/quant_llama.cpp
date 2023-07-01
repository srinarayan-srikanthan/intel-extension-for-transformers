// Defines sigaction on msys:
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdint.h>
#include <cstdio>
#include <map>
#include <string>
#include <exception>
#include <utility>

#include "models/llama/llama_model.h"

static const std::map<std::string, model_ftype> NE_FTYPE_MAP = {
    {"q4_0", MODEL_FTYPE_MOSTLY_Q4_0},
    {"q4_1", MODEL_FTYPE_MOSTLY_Q4_1},
    {"q5_0", MODEL_FTYPE_MOSTLY_Q5_0},
    {"q5_1", MODEL_FTYPE_MOSTLY_Q5_1},
    {"q8_0", MODEL_FTYPE_MOSTLY_Q8_0},
    {"q4_j_b32", MODEL_FTYPE_MOSTLY_Q4_JBLAS_B32},
    {"q4_j_b128", MODEL_FTYPE_MOSTLY_Q4_JBLAS_B128},
    {"q4_j_b1024", MODEL_FTYPE_MOSTLY_Q4_JBLAS_B1024},
    {"q4_j_bf16_b32", MODEL_FTYPE_MOSTLY_Q4_JBLAS_BF16_B32},

};

bool try_parse_ftype(const std::string& ftype_str, model_ftype& ftype, std::string& ftype_str_out) {
  auto it = NE_FTYPE_MAP.find(ftype_str);
  if (it != NE_FTYPE_MAP.end()) {
    ftype = it->second;
    ftype_str_out = it->first;
    return true;
  }
  // try to parse as an integer
  try {
    int ftype_int = std::stoi(ftype_str);
    for (auto it = NE_FTYPE_MAP.begin(); it != NE_FTYPE_MAP.end(); it++) {
      if (it->second == ftype_int) {
        ftype = it->second;
        ftype_str_out = it->first;
        return true;
      }
    }
  } catch (...) {
    // stoi failed
  }
  return false;
}

// usage:
//  ./quantize models/llama/ne-model.bin [models/llama/ne-model-quant.bin] type [nthreads]
//
int main(int argc, char** argv) {
  if (argc < 3) {
    fprintf(stderr, "usage: %s model-f32.bin [model-quant.bin] type [nthreads]\n", argv[0]);
    for (auto it = NE_FTYPE_MAP.begin(); it != NE_FTYPE_MAP.end(); it++) {
      fprintf(stderr, "  type = \"%s\" or %d\n", it->first.c_str(), it->second);
    }
    return 1;
  }

  model_init_backend();

  // parse command line arguments
  const std::string fname_inp = argv[1];
  std::string fname_out;
  int nthread;
  model_ftype ftype;

  int arg_idx = 2;
  std::string ftype_str;
  if (try_parse_ftype(argv[arg_idx], ftype, ftype_str)) {
    // argv[2] is the ftype
    std::string fpath;
    const size_t pos = fname_inp.find_last_of('/');
    if (pos != std::string::npos) {
      fpath = fname_inp.substr(0, pos + 1);
    }
    // export as [inp path]/ne-model-[ftype].bin
    fname_out = fpath + "ne-model-" + ftype_str + ".bin";
    arg_idx++;
  } else {
    // argv[2] is the output path
    fname_out = argv[arg_idx];
    arg_idx++;

    if (argc <= arg_idx) {
      fprintf(stderr, "%s: missing ftype\n", __func__);
      return 1;
    }
    // argv[3] is the ftype
    if (!try_parse_ftype(argv[arg_idx], ftype, ftype_str)) {
      fprintf(stderr, "%s: invalid ftype '%s'\n", __func__, argv[3]);
      return 1;
    }
    arg_idx++;
  }

  // parse nthreads
  if (argc > arg_idx) {
    try {
      nthread = std::stoi(argv[arg_idx]);
    } catch (const std::exception& e) {
      fprintf(stderr, "%s: invalid nthread '%s' (%s)\n", __func__, argv[arg_idx], e.what());
      return 1;
    }
  } else {
    nthread = 0;
  }

  fprintf(stderr, "%s: quantizing '%s' to '%s' as %s", __func__, fname_inp.c_str(), fname_out.c_str(),
          ftype_str.c_str());
  if (nthread > 0) {
    fprintf(stderr, " using %d threads", nthread);
  }
  fprintf(stderr, "\n");

  const int64_t t_main_start_us = model_time_us();

  int64_t t_quantize_us = 0;

  // load the model
  {
    const int64_t t_start_us = model_time_us();

    if (model_model_quantize(fname_inp.c_str(), fname_out.c_str(), ftype, nthread)) {
      fprintf(stderr, "%s: failed to quantize model from '%s'\n", __func__, fname_inp.c_str());
      return 1;
    }

    t_quantize_us = model_time_us() - t_start_us;
  }

  // report timing
  {
    const int64_t t_main_end_us = model_time_us();

    printf("\n");
    printf("%s: quantize time = %8.2f ms\n", __func__, t_quantize_us / 1000.0);
    printf("%s:    total time = %8.2f ms\n", __func__, (t_main_end_us - t_main_start_us) / 1000.0);
  }

  return 0;
}