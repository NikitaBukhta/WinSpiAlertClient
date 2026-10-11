#ifndef WINSPIALERTCLIENT_COMSCOPE_H
#define WINSPIALERTCLIENT_COMSCOPE_H

namespace utils
{

class ComScope
{
public:
	ComScope();
	~ComScope();

	ComScope(const ComScope& other) = delete;
	ComScope(ComScope&& other) noexcept = delete;
	ComScope& operator=(const ComScope& other) = delete;
	ComScope& operator=(ComScope&& other) noexcept = delete;

	[[nodiscard]] bool is_initialized() const noexcept;

private:
	bool _initialized{};
	bool _must_uninitialize{};
};

} // namespace utils

#endif // WINSPIALERTCLIENT_COMSCOPE_H
