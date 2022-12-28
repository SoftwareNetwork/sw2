#pragma once

#include "helpers.h"

#include "input.h"

namespace sw {

struct command_line_parser {
    struct arg {
        string_view value;
        mutable bool consumed{};
        operator auto() const { return value; }
        //operator const char *() const { return value.data(); }
        auto c_str() const { return value.data(); }
    };
    struct args {
        std::vector<arg> value;
        auto active() const { return value | std::views::filter([](auto &&v){return !v.consumed;}); }
        bool empty() const { return size() == 0; }
        size_t size() const { return std::ranges::count_if(value, [](auto &&v){return !v.consumed;}); }
        const arg &operator[](int i) const { return value[i]; }
    };

    template <auto OptionName, typename T, auto nargs = 1>
    struct argument {
        std::optional<T> value;

        static constexpr string_view option_name() {
            return OptionName;
        }

        explicit operator bool() const {
            return !!value;
        }
        operator T() const {
            return *value;
        }
        void parse(auto &&args) {
            if (args.size() < nargs) {
                throw std::runtime_error{format("no value for argument {}", option_name())};
            }
            if constexpr (std::same_as<T, bool>) {
                value = true;
            } else {
                value = args[0].c_str();
                //args[0].consumed = true;
                SW_UNIMPLEMENTED;
                //args = args.subspan(nargs);
            }
        }
    };
    template <auto OptionName, auto ... Aliases> struct flag {
        bool value;

        flag() : value{false} {}

        static bool is_option_flag(auto &&arg) {
            return option_name() == arg || (((string_view)Aliases == arg) || ... || false);
        }
        static constexpr string_view option_name() {
            return OptionName;
        }
        explicit operator bool() const {
            return value;
        }
        void parse(auto &&args) {
            value = true;
        }
    };

    // subcommands

    struct build {
        static constexpr inline auto name = "build"sv;

        input i; // inputs
        flag<"-static"_s> static_;
        flag<"-shared"_s> shared;
        flag<"-c_static_runtime"_s> c_static_runtime;
        flag<"-cpp_static_runtime"_s> cpp_static_runtime;
        flag<"-mt"_s> c_and_cpp_static_runtime; // windows compat

        auto options() {
            return std::tie(
                static_,
                shared,
                c_static_runtime,
                cpp_static_runtime,
                c_and_cpp_static_runtime
            );
        }

        void parse(const args &args) {
            auto check_spec = [&](auto &&fn) {
                if (fs::exists(fn)) {
                    i = specification_file_input{fn};
                    return true;
                }
                return false;
            };
            0 || check_spec("sw.h")
              || check_spec("sw.cpp") // old compat. After rewrite remove sw.h
              //|| check_spec("sw2.cpp")
              //|| (i = directory_input{"."}, true)
                ;
            parse1(*this, args);
        }
    };
    struct test {
        static constexpr inline auto name = "test"sv;

        void parse(auto &&args) {
        }
    };
    struct generate {
        static constexpr inline auto name = "generate"sv;

        void parse(auto &&args) {
        }
    };
    using command_types = types<build, generate, test>;
    using command = command_types::variant_type;

    command c;
    argument<"-d"_s, path> working_directory;
    flag<"-sw1"_s> sw1; // not a driver, but a real invocation
    flag<"-sfc"_s> save_failed_commands;
    flag<"-sec"_s> save_executed_commands;

    auto options() {
        return std::tie(
            working_directory,
            sw1,
            save_failed_commands,
            save_executed_commands
        );
    }

    command_line_parser(int argc, char *argv[]) {
        args a{.value{(const char **)argv, (const char **)argv + argc}};
        parse(a);
        // run()?
    }
    void parse(const args &args) {
        if (args.size() <= 1) {
            throw std::runtime_error{"no command was issued"};
        }
        args[0].consumed = true;
        parse1(*this, args, false);
    }
    static bool is_option_flag(auto &&opt, auto &&arg) {
        if constexpr (requires {opt.is_option_flag(arg);}) {
            return opt.is_option_flag(arg);
        }
        return arg == opt.option_name();
    }
    static void parse1(auto &&obj, auto &&args, bool command = true) {
        using type = std::decay_t<decltype(obj)>;
        // pre command
        for (auto &&a : args.active()) {
            a.consumed = true;
            bool iscmd{};
            if constexpr (requires { obj.c; }) {
                iscmd = command || type::command_types::for_each([&]<typename T>(T **) {
                    if (T::name == a) {
                        obj.c = T{};
                        std::get<T>(obj.c).parse(args);
                        return true;
                    }
                    return false;
                });
            }
            if (iscmd) {
                break;
            }
            bool parsed{};
            std::apply(
                [&](auto &&...opts) {
                    auto f = [&](auto &&opt) {
                        if (is_option_flag(opt, a)) {
                            opt.parse(args);
                            parsed = true;
                        }
                    };
                    (f(FWD(opts)), ...);
                },
                obj.options());
            if (!parsed) {
                if (command) {
                    a.consumed = false;
                }
            }
        }
        if (command) {
            return;
        }
        // post command
        for (auto &&a : args.active()) {
            // no command here
            bool parsed{};
            std::apply(
                [&](auto &&...opts) {
                    auto f = [&](auto &&opt) {
                        if (is_option_flag(opt, a)) {
                            a.consumed = true;
                            opt.parse(args);
                            parsed = true;
                        }
                    };
                    (f(FWD(opts)), ...);
                },
                obj.options());
            if (!parsed) {
                std::cerr << "unknown option: " << a.value << "\n";
            }
        }
    }
};

} // namespace sw
