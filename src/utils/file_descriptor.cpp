#include "file_descriptor.hpp"

#include <unistd.h>
#include <utility>

namespace clipdeck {

FileDescriptor::FileDescriptor(int descriptor) : descriptor_(descriptor) {}

FileDescriptor::~FileDescriptor() { Reset(); }

FileDescriptor::FileDescriptor(FileDescriptor &&other) noexcept
    : descriptor_(std::exchange(other.descriptor_, -1)) {}

FileDescriptor &FileDescriptor::operator=(FileDescriptor &&other) noexcept {
  if (this != &other) {
    Reset(std::exchange(other.descriptor_, -1));
  }

  return *this;
}

int FileDescriptor::Get() const { return descriptor_; }

bool FileDescriptor::IsValid() const { return descriptor_ >= 0; }

int FileDescriptor::Release() { return std::exchange(descriptor_, -1); }

void FileDescriptor::Reset(int descriptor) {
  if (descriptor_ >= 0) {
    close(descriptor_);
  }

  descriptor_ = descriptor;
}

} // namespace clipdeck
