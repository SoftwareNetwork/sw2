#pragma once

#include "helpers.h"

#include "input.h"

namespace sw {

struct command_line_parser {
    using args = std::span<string_view>;

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
                throw std::runtime_error{std::format("no value for argument {}", option_name())};
            }
            if constexpr (std::same_as<T, bool>) {
                value = true;
            } else {
                value = args[0];
                args = args.subspan(nargs);
            }
        }
    };
    template <auto OptionName, auto ... Aliases> struct flag {
        bool value{};

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

        void parse(auto &&args) {
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
    flag<"-sfc"_s> sfc;
    flag<"-sec"_s> sec;

    auto options() {
        return std::tie(
            working_directory,
            sw1,
            sfc,
            sec
        );
    }

    command_line_parser(std::span<string_view> args) {
        parse(args);
        // run()?
    }
    void parse(args args) {
        if (args.size() <= 1) {
            throw std::runtime_error{"no command was issued"};
        }
        parse1(*this, args.subspan(1));
    }
    static bool is_option_flag(auto &&opt, auto &&arg) {
        if constexpr (requires {opt.is_option_flag(arg);}) {
            return opt.is_option_flag(arg);
        }
        return arg == opt.option_name();
    }
    static void parse1(auto &&obj, auto &&args) {
        using type = std::decay_t<decltype(obj)>;
        while (!args.empty()) {
            bool iscmd{};
            if constexpr (requires {obj.c;}) {
                iscmd = type::command_types::for_each([&]<typename T>(T **) {
                    if (T::name == args[0]) {
                        obj.c = T{};
                        std::get<T>(obj.c).parse(args = args.subspan(1));
                        return true;
                    }
                    return false;
                });
            }
            if (!iscmd) {
                bool parsed{};
                std::apply(
                    [&](auto &&...opts) {
                        auto f = [&](auto &&opt) {
                            if (is_option_flag(opt, args[0])) {
                                opt.parse(args = args.subspan(1));
                                parsed = true;
                            }
                        };
                        (f(FWD(opts)), ...);
                    },
                    obj.options());
                if (!parsed) {
                    std::cerr << "unknown option: " << args[0] << "\n";
                    args = args.subspan(1);
                }
            }
        }
    }
};

} // namespace sw
