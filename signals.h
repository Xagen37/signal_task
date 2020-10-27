#pragma once
#include <functional>
#include "intrusive_list.h"

namespace signals
{

	template <typename T>
	struct signal;

	template <typename... Args>
	struct signal<void(Args...)>
	{
		using slot_t = std::function<void(Args...)>;

		struct connection : intrusive::list_element<struct connection_tag>
		{
			connection() = default;
			connection(signal* sig, slot_t slot) noexcept;

			connection(connection const&) = delete;
			connection& operator=(connection const&) = delete;

			connection(connection&&) noexcept;
			connection& operator=(connection&&) noexcept;

			~connection();

			void disconnect() noexcept;

			friend struct signal;

		private:
			void replace(connection& other);

			signal* sig = nullptr;
			slot_t slot = nullptr;
		};

		using connections_t = intrusive::list<connection, struct connection_tag>;

		struct iteration_token
		{
			explicit iteration_token(signal const* sig) noexcept;

			iteration_token(const iteration_token&) = delete;
			iteration_token& operator=(const iteration_token&) = delete;

			~iteration_token();

		private:
			signal const* sig;
			typename connections_t::const_iterator it;

			iteration_token* next;

			friend struct signal;
		};

		signal() = default;

		signal(signal const&) = delete;
		signal& operator=(signal const&) = delete;

		~signal();

		connection connect(std::function<void(Args...)> slot) noexcept;

		void operator()(Args...) const;

	private:
		connections_t connections;
		mutable iteration_token* top_token = nullptr;
	};


//connection realisation:

	template <typename... Args>
	signal<void(Args...)>::connection::connection(signal* sig, std::function<void(Args...)> slot) noexcept
		: sig(sig)
		, slot(std::move(slot))
	{
		if (sig) { sig->connections.push_front(*this); }
	}

	template <typename... Args>
	signal<void(Args...)>::connection::connection(connection&& other) noexcept
		: sig(other.sig)
		, slot(std::move(other.slot))
	{
		replace(other);
	}

	template <typename... Args>
	typename signal<void(Args...)>::connection& signal<void(Args...)>::connection::operator=(connection&& other) noexcept
	{
		if (this == &other) { return *this; }

		disconnect();
		sig = other.sig;
		slot = std::move(other.slot);
		replace(other);

		return *this;
	}

	template <typename... Args>
	signal<void(Args...)>::connection::~connection()
	{
		if (sig) { disconnect(); }
	}

	template<typename... Args>
	void signal<void(Args...)>::connection::replace(connection& other)
	{
		if (!sig) { return; }

		sig->connections.insert(sig->connections.as_iterator(other), *this);
		other.disconnect();
	}

	template <typename... Args>
	void signal<void(Args...)>::connection::disconnect() noexcept
	{
		if (!sig) { return; }

		for (iteration_token* tok = sig->top_token; tok != nullptr && tok->sig; tok = tok->next)
		{
			if (&*tok->it == this)
			{
				tok->it--;
			}
		}

		unlink();
		sig = nullptr;
	}


//iteration_token realisation:

	template <typename... Args>
	signal<void(Args...)>::iteration_token::iteration_token(signal const* sig) noexcept
		: sig(sig)
	{
		if (sig)
		{
			it = sig->connections.begin();
			next = sig->top_token;
			sig->top_token = this;
		}
	}

	template <typename... Args>
	signal<void(Args...)>::iteration_token::~iteration_token()
	{
		if (sig) { sig->top_token = next; }
	}


//signal realisation:

	template <typename... Args>
	typename signals::signal<void(Args...)>::connection
	signal<void(Args...)>::connect(std::function<void(Args...)> slot) noexcept
	{
		return signals::signal<void(Args...)>::connection(this, std::move(slot));
	}

	// emit
	template <typename... Args> 
	void signal<void(Args...)>::operator()(Args... args) const 
	{
		iteration_token token(this);

		while (token.sig && token.it != connections.end())
		{
			if (token.it->slot)
			{
				(token.it->slot)(args...);
			}

			token.it++;
		}
	}

	template <typename... Args>
	signal<void(Args...)>::~signal() 
	{
		for (auto connection_it = connections.begin(); connection_it != connections.end(); )
		{
			auto copy_it = connection_it;
			connection_it++;
			copy_it->disconnect();
		}

		for (iteration_token* tok = top_token; tok != nullptr; tok = tok->next)
		{
			tok->sig = nullptr;
		}
	}
}