
class bad_optional_access : public std::logic_error
{
public:
	bad_optional_access(const char*msg)
		: logic_error(msg)
	{

	}
};
template <class T>
class optional : public std::tuple<bool, T>
{
private:
	typedef std::tuple<bool, T> base;
public:
	typedef T value_type;

	optional(const T &value)
		: base(true, value)
	{
	}
	optional()
		: base(false, {})
	{
	}
	optional(const base& value)
		: base(value)
	{
	}
	bool has_value() const
	{
		return std::get<0>(*this);
	}
	explicit operator bool() const
	{
		return has_value();
	}
	T& value() &
	{
		if (!has_value())
			throw bad_optional_access("object hasn't value");
		return std::get<1>(*this);
	}
	const T & value() const &
	{
		if (!has_value())
			throw bad_optional_access("object hasn't value");
		return std::get<1>(*this);
	}
	const T* operator->() const
	{
		if (!has_value())
			throw bad_optional_access("object hasn't value");
		return &get<1>();
	}
	T* operator->()
	{
		if (!has_value())
			throw bad_optional_access("object hasn't value");
		return &get<1>();
	}
	const T& operator*() const&
	{
		return value();
	}
	T& operator*() &
	{
		return value();
	}
};
