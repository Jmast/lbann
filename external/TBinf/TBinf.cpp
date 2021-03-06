////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2014-2016, Lawrence Livermore National Security, LLC. 
// Produced at the Lawrence Livermore National Laboratory. 
// Written by the LBANN Research Team (B. Van Essen, et al.) listed in
// the CONTRIBUTORS file. <lbann-dev@llnl.gov>
//
// LLNL-CODE-697807.
// All rights reserved.
//
// This file is part of LBANN: Livermore Big Artificial Neural Network
// Toolkit. For details, see http://software.llnl.gov/LBANN or
// https://github.com/LLNL/LBANN. 
//
// Licensed under the Apache License, Version 2.0 (the "Licensee"); you
// may not use this file except in compliance with the License.  You may
// obtain a copy of the License at:
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied. See the License for the specific language governing
// permissions and limitations under the license.
//
// TBinf.cpp - Tensorboard interface implementation
////////////////////////////////////////////////////////////////////////////////

#include <string.h>
#include <algorithm>
#include <chrono>
#include <limits>
#include "TBinf.hpp"
#include "tbext.hpp"

namespace TBinf {

SummaryWriter::SummaryWriter(std::string logdir) {
  GOOGLE_PROTOBUF_VERIFY_VERSION;
  filename = logdir + "/events.tfevents.";
  double secs = get_time_in_seconds();
  filename += std::to_string((int64_t) secs);
  // Note: Tensorflow also appends the hostname here, but that doesn't currently
  // seem necessary.
  // TODO: We might check whether the file exists.
  file.open(filename, std::ios::out | std::ios::trunc | std::ios::binary);
  // Write an initial event with the version.
  tensorflow::Event e;
  e.set_wall_time(secs);
  e.set_file_version(EVENT_VERSION);
  write_event(e);
  flush();
}

SummaryWriter::~SummaryWriter() {
  flush();
  file.close();
}

void SummaryWriter::add_scalar(std::string tag, float value, int64_t step) {
  // Allocation is freed after the event takes ownership.
  tensorflow::Summary* s = new tensorflow::Summary();
  tensorflow::Summary::Value* v = s->add_value();
  v->set_tag(tag);
  v->set_simple_value(value);
  write_summary_event(s, step);
}

void SummaryWriter::add_histogram(std::string tag, std::vector<double> values,
                                  int64_t step) {
  double min = std::numeric_limits<double>::infinity();
  double max = -std::numeric_limits<double>::infinity();
  double num = 0.0;
  double sum = 0.0;
  double sqsum = 0.0;
  // Set up the buckets.
  std::vector<double> pos_buckets;
  std::vector<double> neg_buckets;
  for (double v = 1.0e-12; v < 1.0e20; v *= 1.1) {
    pos_buckets.push_back(v);
    neg_buckets.push_back(-v);
  }
  pos_buckets.push_back(std::numeric_limits<double>::max());
  neg_buckets.push_back(-std::numeric_limits<double>::max());
  std::reverse(neg_buckets.begin(), neg_buckets.end());
  std::vector<double> bucket_limits;
  bucket_limits.insert(bucket_limits.end(), neg_buckets.begin(),
                       neg_buckets.end());
  bucket_limits.push_back(0.0);
  bucket_limits.insert(bucket_limits.end(), pos_buckets.begin(),
                       pos_buckets.end());
  std::vector<double> buckets(bucket_limits.size(), 0.0);
  // Compute stats and buckets.
  for (const auto& val : values) {
    int bucket = std::upper_bound(bucket_limits.begin(), bucket_limits.end(),
                                  val) - bucket_limits.begin();
    buckets[bucket] += 1.0;
    if (val < min) {
      min = val;
    }
    if (val > max) {
      max = val;
    }
    num += 1.0;
    sum += val;
    sqsum += val * val;
  }
  // Set up the summary object.
  tensorflow::Summary* s = new tensorflow::Summary();
  tensorflow::Summary::Value* v = s->add_value();
  v->set_tag(tag);
  tensorflow::HistogramProto* histo = v->mutable_histo();
  histo->Clear();
  histo->set_min(min);
  histo->set_max(max);
  histo->set_num(num);
  histo->set_sum(sum);
  histo->set_sum_squares(sqsum);
  for (size_t i = 0; i < buckets.size();) {
    double end = bucket_limits[i];
    double count = buckets[i];
    ++i;
    while (i < buckets.size() && buckets[i] <= 0.0) {
      end = bucket_limits[i];
      count = buckets[i];
      ++i;
    }
    histo->add_bucket_limit(end);
    histo->add_bucket(count);
  }
  if (histo->bucket_size() == 0.0) {
    histo->add_bucket_limit(std::numeric_limits<double>::max());
    histo->add_bucket(0.0);
  }
  write_summary_event(s, step);
}

void SummaryWriter::flush() {
  file.flush();
}

void SummaryWriter::write_summary_event(tensorflow::Summary* s, int64_t step) {
  tensorflow::Event e;
  e.set_wall_time(get_time_in_seconds());
  if (step >= 0) {
    e.set_step(step);
  }
  e.set_allocated_summary(s);
  write_event(e);
}

void SummaryWriter::write_event(tensorflow::Event& e) {
  // Record format (from Tensorflow record_writer.cc):
  // uint64 length
  // uint32 masked crc of length
  // byte data[length]
  // uint32 masked crc of data
  std::string serialized;
  e.SerializeToString(&serialized);
  char header[sizeof(uint64_t) + sizeof(uint32_t)];
  char footer[sizeof(uint32_t)];
  put64(header, serialized.size());
  put32(header + sizeof(uint64_t), masked_crc32(header, sizeof(uint64_t)));
  put32(footer, masked_crc32(serialized.data(), serialized.size()));
  file << std::string(header, sizeof(header));
  file << serialized;
  file << std::string(footer, sizeof(footer));
}

double SummaryWriter::get_time_in_seconds() {
  using namespace std::chrono;
  auto now = system_clock::now().time_since_epoch();
  return duration_cast<duration<double>>(now).count();
}

}  // namespace TBinf
