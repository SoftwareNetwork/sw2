#pragma once

#include "helpers.h"

#include "input.h"

namespace sw {

struct command_line_parser {
    using args = std::span<string_view>;
    struct build {
        static constexpr inline auto name = "build"sv;

        input i;

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
        }
    };
    struct generate {
        static constexpr inline auto name = "generate"sv;

        void parse(auto &&args) {
        }
    };
    using command_types = types<build, generate>;
    using command = command_types::variant_type;

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
            value = args[0];
            args = args.subspan(nargs);
        }
    };

    command c;
    argument<"-d"_s, path> working_directory;

    auto options() {
        return std::tie(working_directory);
    }

    command_line_parser(int argc, char *argv[]) {
        std::vector<string_view> args((const char **)argv, (const char **)argv + argc);
        parse(args);
        // run()?
    }
    void parse(args args) {
        if (args.size() <= 1) {
            throw std::runtime_error{"no command was issued"};
        }
        parse1(args.subspan(1));
    }
    void parse1(auto &&args) {
        while (!args.empty()) {
            auto iscmd = command_types::for_each([&]<typename T>(T **) {
                if (T::name == args[0]) {
                    c = T{};
                    std::get<T>(c).parse(args = args.subspan(1));
                    return true;
                }
                return false;
            });
            if (!iscmd) {
                std::apply(
                    [&](auto &&...opts) {
                        auto f = [&](auto &&opt) {
                            if (args[0] == opt.option_name()) {
                                opt.parse(args = args.subspan(1));
                            }
                        };
                        (f(FWD(opts)), ...);
                    },
                    options());
            }
        }
    }
};

} // namespace sw
