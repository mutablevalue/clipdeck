#pragma once

namespace clipdeck {

class FileDescriptor {
public:
  FileDescriptor() = default;
  explicit FileDescriptor(int descriptor);
  ~FileDescriptor();

  FileDescriptor(const FileDescriptor &) = delete;
  FileDescriptor &operator=(const FileDescriptor &) = delete;

  FileDescriptor(FileDescriptor &&other) noexcept;
  FileDescriptor &operator=(FileDescriptor &&other) noexcept;

  [[nodiscard]] int Get() const;
  [[nodiscard]] bool IsValid() const;
  int Release();
  void Reset(int descriptor = -1);

private:
  int descriptor_ = -1;
};

} // namespace clipdeck
