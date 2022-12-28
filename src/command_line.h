#pragma once

#include "helpers.h"

#include "input.h"

namespace sw {

struct command_line_parser {
    struct options {
        struct comma_separated_value {};
        template <auto AliasName1, auto ... Aliases>
        struct aliases {
        };
        template <auto From, auto To>
        struct nargs {
        };
    };

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
        auto active(auto &from) const {
            return std::ranges::subrange{value.begin() + (&from - &value[0]), value.end()}
            | std::views::filter([](auto &&v){return !v.consumed;});
        }
        auto &get_next(auto &from) const {
            if (empty()) {
                throw std::runtime_error{"missing argument on the command line"};
            }
            auto a = active(from);
            return *std::begin(a);
        }
        auto &get_next() const {
            if (empty()) {
                throw std::runtime_error{"missing argument on the command line"};
            }
            return get_next(*value.begin());
        }
        bool empty() const { return size() == 0; }
        size_t size() const { return std::ranges::count_if(value, [](auto &&v){return !v.consumed;}); }
        const arg &operator[](int i) const { return value[i]; }
    };

    template <auto OptionName, typename T, auto ... Options>
    struct argument {
        std::optional<T> value;

        static constexpr string_view option_name() {
            return OptionName;
        }

        explicit operator bool() const {
            return !!value;
        }
        operator auto() const {
            return *value;
        }
        void parse(auto &&args, auto &&cur) {
            /*if (args.size() < nargs) {
                throw std::runtime_error{format("no value for argument {}", option_name())};
            }*/
            if constexpr (std::same_as<T, bool>) {
                value = true;
                return;
            }
            auto &a = args.get_next(cur);
            if constexpr (std::same_as<T, int>) {
                value = std::stoi(a.c_str());
            } else {
                value = a.c_str();
                //SW_UNIMPLEMENTED;
                //args = args.subspan(nargs);
            }
            a.consumed = true;
        }
    };
    template <auto OptionName, auto ... Options> struct flag {
        bool value;

        flag() : value{false} {}

        static bool is_option_flag(auto &&arg) {
            return option_name() == arg;// || (((string_view)Aliases == arg) || ... || false);
        }
        static constexpr string_view option_name() {
            return OptionName;
        }
        explicit operator bool() const {
            return value;
        }
        void parse(auto &&args, auto &&) {
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
        flag<"-mt"_s, options::aliases<"-c_and_cpp_static_runtime"_s>{}> c_and_cpp_static_runtime; // windows compat
        flag<"-md"_s, options::aliases<"-c_and_cpp_dynamic_runtime"_s>{}> c_and_cpp_dynamic_runtime; // windows compat
        argument<"-arch"_s, string, options::comma_separated_value{}> arch;
        argument<"-config"_s, string, options::comma_separated_value{}> config;

        auto options() {
            return std::tie(
                static_,
                shared,
                c_static_runtime,
                cpp_static_runtime,
                c_and_cpp_static_runtime,
                c_and_cpp_dynamic_runtime,
                arch,
                config
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
    // some debug
    argument<"-sleep"_s, int> sleep;
    flag<"-int3"_s> int3;

    auto options() {
        return std::tie(
            sleep,
            int3,

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
                            opt.parse(args, a);
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
                            opt.parse(args, a);
                            parsed = true;
                        }
                    };
                    (f(FWD(opts)), ...);
                },
                obj.options());
            if (!parsed) {
                throw std::runtime_error{format("unknown option: {}", a.value)};
            }
        }
    }
};

} // namespace sw
