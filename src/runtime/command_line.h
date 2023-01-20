#pragma once

#include "../helpers/common.h"

namespace sw {

// gcc issue 1 - cannot be used inside class
template <auto ... Options>
static consteval bool is_single_val1() {
    constexpr auto is_multi = [&](auto &&v) constexpr {
        // gcc issue 2 - is_single does not work as function is_single()
        if constexpr (requires { v.is_single; }) {
            return !v.is_single;
        }
        return false;
    };
    constexpr bool r = !(is_multi(Options) || ... || false);
    return r;
}

// also gcc issue, cant be used inside argument class
template <typename U, auto ... Options> static constexpr bool is() {
    auto f = [&](auto &&v) {
        return std::same_as<U, std::decay_t<decltype(v)>>;
    };
    return (f(Options) || ... || false);
}

struct command_line_parser {
    struct options {
        template <auto Name, auto ... Aliases>
        struct flag {
            static bool is_option_flag(auto &&arg) {
                return (string_view)Name == arg || (((string_view)Aliases == arg) || ... || false);
            }
        };
        struct consume_after {};
        struct positional {};
        struct comma_separated_value {};
        template <auto Min, auto Max>
        struct nargs {
            static constexpr auto min() { return Min; }
            static constexpr auto max() { return Max; }
            static inline constexpr auto is_single = Min == 1 && Max == 1;
        };
        struct zero_or_more : nargs<0, -1> {};
        struct one_or_more : nargs<1, -1> {};
    };

    struct arg {
        string_view value;
        mutable bool consumed{};
        operator auto() const { return value; }
        //operator const char *() const { return value.data(); }
        auto c_str() const { return value.data(); }
        auto str() const { return string(value.begin(), value.end()); }
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

    template <typename T, auto ... Options>
    struct argument {
        static inline constexpr bool is_single_val = is_single_val1<Options...>() && !is<options::consume_after, Options...>();
        using value_type = std::conditional_t<is_single_val, T, std::vector<T>>;

        std::optional<value_type> value;

        static bool is_option_flag(auto &&arg) {
            auto f = [&](auto &&v) {
                if constexpr (requires {v.is_option_flag(arg);}) {
                    return v.is_option_flag(arg);
                }
                return false;
            };
            return (f(Options) || ... || false);
        }
        static constexpr bool is_positional() {
            return is<options::positional, Options...>();
        }

        explicit operator bool() const {
            return !!value;
        }
        operator auto() const {
            return *value;
        }
        void parse(auto &&args, auto &&cur) requires (is_single_val) {
            if constexpr (std::same_as<T, bool>) {
                value = true;
                return;
            }
            auto &a = args.get_next(cur);
            if constexpr (std::same_as<T, int>) {
                value = std::stoi(a.c_str());
            } else {
                value = a.c_str();
            }
            a.consumed = true;
        }
        void parse(auto &&args, auto &&cur) requires (!is_single_val) {
            auto consume_after = is<options::consume_after>();
            for (auto &&a : args.active()) {
                if (consume_after) {
                    if (!value) {
                        value = value_type{};
                    }
                    value->push_back(a.str());
                    a.consumed = true;
                    continue;
                }
                if (a.value[0] == '-') {
                    break;
                }
                if (!value) {
                    value = value_type{};
                }
                if constexpr (std::same_as<T, int>) {
                    value->push_back(std::stoi(a.str()));
                } else {
                    value->push_back(a.str());
                }
                a.consumed = true;
            }
        }
        template <auto Min, auto Max>
        void check_nargs1(const options::nargs<Min,Max> &) {
            if (Min > 0) {
                if (!value || value->size() < Min) {
                    throw std::runtime_error{"not enough args"};
                }
            }
        }
        void check_nargs1(const options::one_or_more &v) {
            if (v.min() > 0) {
                if (!value || value->size() < v.min()) {
                    throw std::runtime_error{"not enough args"};
                }
            }
        }
        void check_nargs1(auto &&) {
        }
        void check() {
            (check_nargs1(Options), ...);
        }
    };
    template <auto FlagName, auto ... Options> struct flag {
        bool value;

        // gcc issue
        flag() : value{false} {}

        static bool is_option_flag(auto &&arg) {
            return FlagName.is_option_flag(arg);
        }
        template <typename T>
        static bool is() {
            return false;
        }
        static bool is_positional() {
            return false;
        }
        explicit operator bool() const {
            return value;
        }
        void parse(auto &&args, auto &&) {
            value = true;
        }
        void check() {
        }
    };

    // subcommands

    struct build_run_common {
        static constexpr inline auto name = "build"sv;

        flag<options::flag<"-explain-outdated"_s>{}> explain_outdated;
        flag<options::flag<"-static"_s>{}> static_;
        flag<options::flag<"-shared"_s>{}> shared;
        flag<options::flag<"-c_static_runtime"_s>{}> c_static_runtime;
        flag<options::flag<"-cpp_static_runtime"_s>{}> cpp_static_runtime;
        flag<options::flag<"-mt"_s, "-c_and_cpp_static_runtime"_s>{}> c_and_cpp_static_runtime;   // windows compat
        flag<options::flag<"-md"_s, "-c_and_cpp_dynamic_runtime"_s>{}> c_and_cpp_dynamic_runtime; // windows compat
        argument<string, options::flag<"-arch"_s>{}, options::comma_separated_value{}> arch;
        argument<string, options::flag<"-config"_s>{}, options::comma_separated_value{}> config;
        argument<string, options::flag<"-compiler"_s>{}, options::comma_separated_value{}> compiler;
        argument<string, options::flag<"-os"_s>{}, options::comma_separated_value{}> os;
        argument<int, options::flag<"-k"_s>{}> ignore_errors;

        auto option_list(auto &&...args) {
            return std::tie(explain_outdated, static_, shared, c_static_runtime, cpp_static_runtime,
                            c_and_cpp_static_runtime, c_and_cpp_dynamic_runtime, arch, config, compiler, os,
                            ignore_errors, FWD(args)...);
        }
    };
    struct build_common : build_run_common {
        static constexpr inline auto name = "build"sv;

        argument<string, options::positional{}, options::zero_or_more{}> inputs;

        auto option_list(auto && ... args) {
            return build_run_common::option_list(inputs, FWD(args)...);
        }
    };
    struct build : build_common {
        static constexpr inline auto name = "build"sv;
    };
    struct override {
        static constexpr inline auto name = "override"sv;

        //input i; // inputs

        /*auto option_list() {
            return std::tie();
        }*/

        void parse(const args &args) {
            /*auto check_spec = [&](auto &&fn) {
                if (fs::exists(fn)) {
                    i = specification_file_input{fn};
                    return true;
                }
                return false;
            };
            0 || check_spec("sw.h") || check_spec("sw.cpp") // old compat. After rewrite remove sw.h
                                                            //|| check_spec("sw2.cpp")
                                                            //|| (i = directory_input{"."}, true)
                ;
            parse1(*this, args);*/
        }
    };
    struct test : build_common {
        static constexpr inline auto name = "test"sv;

        argument<string, options::flag<"-f"_s>{}> format{"junit"};

        auto option_list() {
            return build_common::option_list(format);
        }
    };
    struct generate : build_common {
        static constexpr inline auto name = "generate"sv;

        argument<string, options::flag<"-g"_s>{}> generator;

        auto option_list() {
            return build_common::option_list(generator);
        }
    };
    struct run_common : build_run_common {
        argument<string, options::positional{}, options::one_or_more{}, options::consume_after{}> arguments;

        auto option_list(auto && ... args) {
            return build_run_common::option_list(arguments, FWD(args)...);
        }
    };
    struct run : run_common {
        static constexpr inline auto name = "run"sv;

        flag<options::flag<"-exec"_s>{}> exec;

        auto option_list() {
            return run_common::option_list(exec);
        }
    };
    // same as run, but exec always
    struct exec : run_common {
        static constexpr inline auto name = "exec"sv;
    };
    using command_types = types<build, generate, test, run, exec>;
    using command = command_types::variant_type;

    command c;
    argument<path, options::flag<"-d"_s>{}> working_directory; // make -C?
    flag<options::flag<"-sw1"_s>{}> sw1; // not a driver, but a real invocation
    flag<options::flag<"-sfc"_s>{}> save_failed_commands;
    flag<options::flag<"-sec"_s>{}> save_executed_commands;
    flag<options::flag<"-B"_s>{}> rebuild_all;
    // some debug
    argument<int, options::flag<"-sleep"_s>{}> sleep;
    argument<int, options::flag<"-j"_s>{}> jobs;
    flag<options::flag<"-int3"_s>{}> int3;
    flag<options::flag<"-trace"_s,"--trace"_s>{}> trace;
    flag<options::flag<"-v"_s,"-verbose"_s,"--verbose"_s>{}> verbose;
    argument<int, options::flag<"-log_level"_s,"--log_level"_s>{}> log_level;
    flag<options::flag<"-version"_s,"--version"_s>{}> version;

    auto option_list() {
        return std::tie(
            sleep,
            int3,
            trace,
            verbose,
            log_level,
            version,

            working_directory,
            sw1,
            save_failed_commands,
            save_executed_commands,
            rebuild_all,
            jobs
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
        return opt.is_option_flag(arg);
    }
    static void parse1(auto &&obj, auto &&args, bool command = true) {
        using type = std::decay_t<decltype(obj)>;
        // pre command
        for (auto &&a : args.active()) {
            bool iscmd{};
            if constexpr (requires { obj.c; }) {
                iscmd = command || type::command_types::for_each([&]<typename T>(T **) {
                    if (T::name == a) {
                        a.consumed = true;
                        obj.c = T{};
                        //std::get<T>(obj.c).parse1(args);
                        parse1(std::get<T>(obj.c), args);
                        return true;
                    }
                    return false;
                });
            }
            if (iscmd) {
                break;
            }
            bool parsed{};
            if constexpr (requires{obj.option_list();}) {
                std::apply(
                    [&](auto &&...opts) {
                        auto f = [&](auto &&opt) {
                            if (is_option_flag(opt, a)) {
                                a.consumed = true;
                                opt.parse(args, a);
                                parsed = true;
                            } else if (a.value[0] != '-' && opt.is_positional()) {
                                opt.parse(args, a);
                                parsed = true;
                            }
                            return parsed;
                        };
                        (f(FWD(opts)) || ... || false);
                    },
                    obj.option_list());
            }
            if (!parsed) {
                if (command) {
                    a.consumed = false;
                } else {
                    throw std::runtime_error{format("unknown option: {}", a.value)};
                }
            }
        }
        // post command
        for (auto &&a : args.active()) {
            // no command here
            bool parsed{};
            if constexpr (requires { obj.option_list(); }) {
                std::apply(
                    [&](auto &&...opts) {
                        auto f = [&](auto &&opt) {
                            if (is_option_flag(opt, a)) {
                                a.consumed = true;
                                opt.parse(args, a);
                                parsed = true;
                            } else if (a.value[0] != '-' && opt.is_positional()) {
                                opt.parse(args, a);
                                parsed = true;
                            }
                            return parsed;
                        };
                        (f(FWD(opts)) || ... || false);
                    },
                    obj.option_list());
            }
            if (!parsed) {
            }
        }
        // now errors
        for (auto &&a : args.active()) {
            if (!command) {
                throw std::runtime_error{format("unknown option: {}", a.value)};
            }
        }

        // do post check
        if constexpr (requires { obj.option_list(); }) {
            std::apply(
                [&](auto &&...opts) {
                    auto f = [&](auto &&opt) {
                    opt.check();
                    };
                    (f(FWD(opts)),...);
                },
                obj.option_list());
        }
    }
};

} // namespace sw
