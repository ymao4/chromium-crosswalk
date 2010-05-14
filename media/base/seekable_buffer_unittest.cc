// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "media/base/data_buffer.h"
#include "media/base/seekable_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class SeekableBufferTest : public testing::Test {
 public:
  SeekableBufferTest() : buffer_(kBufferSize, kBufferSize) {
  }

 protected:
  static const size_t kDataSize = 409600;
  static const size_t kBufferSize = 4096;
  static const size_t kWriteSize = 512;

  virtual void SetUp() {
    // Setup seed.
    size_t seed = static_cast<int32>(base::Time::Now().ToInternalValue());
    srand(seed);
    LOG(INFO) << "Random seed: " << seed;

    // Creates a test data.
    for (size_t i = 0; i < kDataSize; i++)
      data_[i] = static_cast<char>(rand());
  }

  size_t GetRandomInt(size_t maximum) {
    return rand() % maximum + 1;
  }

  media::SeekableBuffer buffer_;
  uint8 data_[kDataSize];
  uint8 write_buffer_[kDataSize];
};

TEST_F(SeekableBufferTest, RandomReadWrite) {
  size_t write_position = 0;
  size_t read_position = 0;
  while (read_position < kDataSize) {
    // Write a random amount of data.
    size_t write_size = GetRandomInt(kBufferSize);
    write_size = std::min(write_size, kDataSize - write_position);
    bool should_append = buffer_.Append(data_ + write_position, write_size);
    write_position += write_size;
    EXPECT_GE(write_position, read_position);
    EXPECT_EQ(write_position - read_position, buffer_.forward_bytes());
    EXPECT_EQ(should_append, buffer_.forward_bytes() < kBufferSize)
        << "Incorrect buffer full reported";

    // Peek a random amount of data.
    size_t copy_size = GetRandomInt(kBufferSize);
    size_t bytes_copied = buffer_.Peek(write_buffer_, copy_size);
    EXPECT_GE(copy_size, bytes_copied);
    EXPECT_EQ(0, memcmp(write_buffer_, data_ + read_position, bytes_copied));

    // Read a random amount of data.
    size_t read_size = GetRandomInt(kBufferSize);
    size_t bytes_read = buffer_.Read(write_buffer_, read_size);
    EXPECT_GE(read_size, bytes_read);
    EXPECT_EQ(0, memcmp(write_buffer_, data_ + read_position, bytes_read));
    read_position += bytes_read;
    EXPECT_GE(write_position, read_position);
    EXPECT_EQ(write_position - read_position, buffer_.forward_bytes());
  }
}

TEST_F(SeekableBufferTest, ReadWriteSeek) {
  const size_t kReadSize = kWriteSize / 4;

  for (int i = 0; i < 10; ++i) {
    // Write until buffer is full.
    for (size_t j = 0; j < kBufferSize; j += kWriteSize) {
      bool should_append = buffer_.Append(data_ + j, kWriteSize);
      EXPECT_EQ(j < kBufferSize - kWriteSize, should_append)
          << "Incorrect buffer full reported";
      EXPECT_EQ(j + kWriteSize, buffer_.forward_bytes());
    }

    // Simulate a read and seek pattern. Each loop reads 4 times, each time
    // reading a quarter of |kWriteSize|.
    size_t read_position = 0;
    size_t forward_bytes = kBufferSize;
    for (size_t j = 0; j < kBufferSize; j += kWriteSize) {
      // Read.
      EXPECT_EQ(kReadSize, buffer_.Read(write_buffer_, kReadSize));
      forward_bytes -= kReadSize;
      EXPECT_EQ(forward_bytes, buffer_.forward_bytes());
      EXPECT_EQ(0, memcmp(write_buffer_, data_ + read_position, kReadSize));
      read_position += kReadSize;

      // Seek forward.
      EXPECT_TRUE(buffer_.Seek(2 * kReadSize));
      forward_bytes -= 2 * kReadSize;
      read_position += 2 * kReadSize;
      EXPECT_EQ(forward_bytes, buffer_.forward_bytes());

      // Copy.
      EXPECT_EQ(kReadSize, buffer_.Peek(write_buffer_, kReadSize));
      EXPECT_EQ(forward_bytes, buffer_.forward_bytes());
      EXPECT_EQ(0, memcmp(write_buffer_, data_ + read_position, kReadSize));

      // Read.
      EXPECT_EQ(kReadSize, buffer_.Read(write_buffer_, kReadSize));
      forward_bytes -= kReadSize;
      EXPECT_EQ(forward_bytes, buffer_.forward_bytes());
      EXPECT_EQ(0, memcmp(write_buffer_, data_ + read_position, kReadSize));
      read_position += kReadSize;

      // Seek backward.
      EXPECT_TRUE(buffer_.Seek(-3 * static_cast<int32>(kReadSize)));
      forward_bytes += 3 * kReadSize;
      read_position -= 3 * kReadSize;
      EXPECT_EQ(forward_bytes, buffer_.forward_bytes());

      // Copy.
      EXPECT_EQ(kReadSize, buffer_.Peek(write_buffer_, kReadSize));
      EXPECT_EQ(forward_bytes, buffer_.forward_bytes());
      EXPECT_EQ(0, memcmp(write_buffer_, data_ + read_position, kReadSize));

      // Read.
      EXPECT_EQ(kReadSize, buffer_.Read(write_buffer_, kReadSize));
      forward_bytes -= kReadSize;
      EXPECT_EQ(forward_bytes, buffer_.forward_bytes());
      EXPECT_EQ(0, memcmp(write_buffer_, data_ + read_position, kReadSize));
      read_position += kReadSize;

      // Copy.
      EXPECT_EQ(kReadSize, buffer_.Peek(write_buffer_, kReadSize));
      EXPECT_EQ(forward_bytes, buffer_.forward_bytes());
      EXPECT_EQ(0, memcmp(write_buffer_, data_ + read_position, kReadSize));

      // Read.
      EXPECT_EQ(kReadSize, buffer_.Read(write_buffer_, kReadSize));
      forward_bytes -= kReadSize;
      EXPECT_EQ(forward_bytes, buffer_.forward_bytes());
      EXPECT_EQ(0, memcmp(write_buffer_, data_ + read_position, kReadSize));
      read_position += kReadSize;

      // Seek forward.
      EXPECT_TRUE(buffer_.Seek(kReadSize));
      forward_bytes -= kReadSize;
      read_position += kReadSize;
      EXPECT_EQ(forward_bytes, buffer_.forward_bytes());
    }
  }
}

TEST_F(SeekableBufferTest, BufferFull) {
  const size_t kMaxWriteSize = 2 * kBufferSize;

  // Write and expect the buffer to be not full.
  for (size_t i = 0; i < kBufferSize - kWriteSize; i += kWriteSize) {
    EXPECT_TRUE(buffer_.Append(data_ + i, kWriteSize));
    EXPECT_EQ(i + kWriteSize, buffer_.forward_bytes());
  }

  // Write until we have kMaxWriteSize bytes in the buffer. Buffer is full in
  // these writes.
  for (size_t i = buffer_.forward_bytes(); i < kMaxWriteSize; i += kWriteSize) {
    EXPECT_FALSE(buffer_.Append(data_ + i, kWriteSize));
    EXPECT_EQ(i + kWriteSize, buffer_.forward_bytes());
  }

  // Read until the buffer is empty.
  size_t read_position = 0;
  while (buffer_.forward_bytes()) {
    // Read a random amount of data.
    size_t read_size = GetRandomInt(kBufferSize);
    size_t forward_bytes = buffer_.forward_bytes();
    size_t bytes_read = buffer_.Read(write_buffer_, read_size);
    EXPECT_EQ(0, memcmp(write_buffer_, data_ + read_position, bytes_read));
    if (read_size > forward_bytes)
      EXPECT_EQ(forward_bytes, bytes_read);
    else
      EXPECT_EQ(read_size, bytes_read);
    read_position += bytes_read;
    EXPECT_GE(kMaxWriteSize, read_position);
    EXPECT_EQ(kMaxWriteSize - read_position, buffer_.forward_bytes());
  }

  // Expects we have no bytes left.
  EXPECT_EQ(0u, buffer_.forward_bytes());
  EXPECT_EQ(0u, buffer_.Read(write_buffer_, 1));
}

TEST_F(SeekableBufferTest, SeekBackward) {
  EXPECT_EQ(0u, buffer_.forward_bytes());
  EXPECT_EQ(0u, buffer_.backward_bytes());
  EXPECT_FALSE(buffer_.Seek(1));
  EXPECT_FALSE(buffer_.Seek(-1));

  const size_t kReadSize = 256;

  // Write into buffer until it's full.
  for (size_t i = 0; i < kBufferSize; i += kWriteSize) {
    // Write a random amount of data.
    buffer_.Append(data_ + i, kWriteSize);
  }

  // Read until buffer is empty.
  for (size_t i = 0; i < kBufferSize; i += kReadSize) {
    EXPECT_EQ(kReadSize, buffer_.Read(write_buffer_, kReadSize));
    EXPECT_EQ(0, memcmp(write_buffer_, data_ + i, kReadSize));
  }

  // Seek backward.
  EXPECT_TRUE(buffer_.Seek(-static_cast<int32>(kBufferSize)));
  EXPECT_FALSE(buffer_.Seek(-1));

  // Read again.
  for (size_t i = 0; i < kBufferSize; i += kReadSize) {
    EXPECT_EQ(kReadSize, buffer_.Read(write_buffer_, kReadSize));
    EXPECT_EQ(0, memcmp(write_buffer_, data_ + i, kReadSize));
  }
}

TEST_F(SeekableBufferTest, GetCurrentChunk) {
  const size_t kSeekSize = kWriteSize / 3;

  scoped_refptr<media::DataBuffer> buffer = new media::DataBuffer(kWriteSize);
  memcpy(buffer->GetWritableData(), data_, kWriteSize);
  buffer->SetDataSize(kWriteSize);

  const uint8* data;
  size_t size;
  EXPECT_FALSE(buffer_.GetCurrentChunk(&data, &size));

  buffer_.Append(buffer.get());
  EXPECT_TRUE(buffer_.GetCurrentChunk(&data, &size));
  EXPECT_EQ(data, buffer->GetData());
  EXPECT_EQ(size, buffer->GetDataSize());

  buffer_.Seek(kSeekSize);
  EXPECT_TRUE(buffer_.GetCurrentChunk(&data, &size));
  EXPECT_EQ(data, buffer->GetData() + kSeekSize);
  EXPECT_EQ(size, buffer->GetDataSize() - kSeekSize);
}

TEST_F(SeekableBufferTest, SeekForward) {
  size_t write_position = 0;
  size_t read_position = 0;
  while (read_position < kDataSize) {
    for (int i = 0; i < 10 && write_position < kDataSize; ++i) {
      // Write a random amount of data.
      size_t write_size = GetRandomInt(kBufferSize);
      write_size = std::min(write_size, kDataSize - write_position);

      bool should_append = buffer_.Append(data_ + write_position, write_size);
      write_position += write_size;
      EXPECT_GE(write_position, read_position);
      EXPECT_EQ(write_position - read_position, buffer_.forward_bytes());
      EXPECT_EQ(should_append, buffer_.forward_bytes() < kBufferSize)
          << "Incorrect buffer full status reported";
    }

    // Read a random amount of data.
    size_t seek_size = GetRandomInt(kBufferSize);
    if (buffer_.Seek(seek_size))
      read_position += seek_size;
    EXPECT_GE(write_position, read_position);
    EXPECT_EQ(write_position - read_position, buffer_.forward_bytes());

    // Read a random amount of data.
    size_t read_size = GetRandomInt(kBufferSize);
    size_t bytes_read = buffer_.Read(write_buffer_, read_size);
    EXPECT_GE(read_size, bytes_read);
    EXPECT_EQ(0, memcmp(write_buffer_, data_ + read_position, bytes_read));
    read_position += bytes_read;
    EXPECT_GE(write_position, read_position);
    EXPECT_EQ(write_position - read_position, buffer_.forward_bytes());
  }
}

TEST_F(SeekableBufferTest, AllMethods) {
  EXPECT_EQ(0u, buffer_.Read(write_buffer_, 0));
  EXPECT_EQ(0u, buffer_.Read(write_buffer_, 1));
  EXPECT_TRUE(buffer_.Seek(0));
  EXPECT_FALSE(buffer_.Seek(-1));
  EXPECT_FALSE(buffer_.Seek(1));
  EXPECT_EQ(0u, buffer_.forward_bytes());
  EXPECT_EQ(0u, buffer_.backward_bytes());
}


TEST_F(SeekableBufferTest, GetTime) {
  const struct {
    int64 first_time_useconds;
    int64 duration_useconds;
    size_t consume_bytes;
    int64 expected_time;
  } tests[] = {
    // Timestamps of 0 are treated as garbage.
    { 0, 1000000, 0, 0 },
    { 0, 4000000, 0, 0 },
    { 0, 8000000, 0, 0 },
    { 0, 1000000, 4, 0 },
    { 0, 4000000, 4, 0 },
    { 0, 8000000, 4, 0 },
    { 0, 1000000, kWriteSize, 0 },
    { 0, 4000000, kWriteSize, 0 },
    { 0, 8000000, kWriteSize, 0 },
    { 5, 1000000, 0, 5 },
    { 5, 4000000, 0, 5 },
    { 5, 8000000, 0, 5 },
    { 5, 1000000, kWriteSize / 2, 500005 },
    { 5, 4000000, kWriteSize / 2, 2000005 },
    { 5, 8000000, kWriteSize / 2, 4000005 },
    { 5, 1000000, kWriteSize, 1000005 },
    { 5, 4000000, kWriteSize, 4000005 },
    { 5, 8000000, kWriteSize, 8000005 },
  };

  scoped_refptr<media::DataBuffer> buffer = new media::DataBuffer(kWriteSize);
  memcpy(buffer->GetWritableData(), data_, kWriteSize);
  buffer->SetDataSize(kWriteSize);

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    buffer->SetTimestamp(base::TimeDelta::FromMicroseconds(
        tests[i].first_time_useconds));
    buffer->SetDuration(base::TimeDelta::FromMicroseconds(
        tests[i].duration_useconds));
    buffer_.Append(buffer.get());
    EXPECT_TRUE(buffer_.Seek(tests[i].consume_bytes));

    int64 actual = buffer_.current_time().ToInternalValue();

    EXPECT_EQ(tests[i].expected_time, actual) << "With test = { start:"
        << tests[i].first_time_useconds << ", duration:"
        << tests[i].duration_useconds << ", consumed:"
        << tests[i].consume_bytes << "}\n";

    buffer_.Clear();
  }
}

}  // namespace
