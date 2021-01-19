#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <any>
#include <variant>
#include <optional>
#include <functional>
#include <array>
#include <algorithm>
#include <sstream>

#ifndef TUPLE_OF_ARGS



namespace tuple_of_args {

    inline namespace detail {
        template<typename T, typename RT = std::void_t<>>
        struct has_return_type :std::false_type {};

        template<typename T>
        struct has_return_type<T, std::void_t<typename T::return_type>> :std::true_type {};


        template<typename T, typename ...List>
        struct is_in_list {
            static constexpr bool value = (std::is_same_v<T, List> || ...);
        };

        template<typename T, typename ...List>
        struct is_not_in_list {
            static constexpr bool value = !(std::is_same_v<T, List> || ...);
        };


        template<typename ...Types>
        struct flat_type_container {
            using type = flat_type_container<Types...>;
            using variant_t = std::variant<Types...>;
            using tuple_t = std::tuple<Types...>;
        };

        template<typename First, typename ...Types>
        struct flat_type_container<First, flat_type_container<Types...>> {
            using type = flat_type_container<First, Types...>;
        };


        template<template<typename> typename Pred>
        struct pred {
            template<typename T>
            struct neg :std::integral_constant<bool, !Pred<T>::value> {};
        };

        //Filtered types
        template<template<typename> typename Filter, typename First, typename ...Rest>
        struct filtered_types {
            using type = typename std::conditional_t <
                Filter<First>::value,
                typename flat_type_container<First, typename filtered_types<Filter, Rest...>::type>::type,
                typename filtered_types<Filter, Rest...>::type
            >;
        };

        template<template<typename> typename Filter, typename First>
        struct filtered_types<Filter, First> {
            using type = typename std::conditional_t <
                Filter<First>::value,
                typename flat_type_container<First>::type,
                typename flat_type_container<>::type>::type;
        };

        template<template<typename, typename> typename Filter, typename First, typename ...Rest>
        struct full_filtered_types {
            using type = typename std::conditional_t <
                Filter<First, Rest...>::value,
                typename flat_type_container<First, typename full_filtered_types<Filter, Rest...>::type>::type,
                typename full_filtered_types<Filter, Rest...>::type
            >;
        };

        template<template<typename, typename> typename Filter, typename First>
        struct full_filtered_types<Filter, First> {
            using type = typename std::conditional_t <
                Filter<First, void>::value,
                typename flat_type_container<First>::type,
                typename flat_type_container<>::type>::type;
        };
        //End filtered types

        //Unique Type Variant
        template<typename ...List>
        struct unique_variant {
            using type = typename full_filtered_types<is_not_in_list, List...>::type::variant_t;
        };

        template<typename ...Items>
        struct unique_variant<flat_type_container<Items...>>
            :unique_variant<typename Items::result_ot...> {};

    }




    using arg_iter_t = std::vector<std::string>::iterator;
    using consumer_t = std::function<std::string(const arg_iter_t&, const arg_iter_t&)>;


    enum class OptionType
    {
        Flag,
        MultiFlag,
        Option,
        Positional
    };

    struct command_type {};
    struct option_type {};

    enum CompareType
    {
        ByPosition,
        ByName,
        ByContext
    };

    bool validCapture(const std::string_view& str) {
        //prefix check

        if (str.length() > 2 && str == "--") return false;
        //allow '-' as a pipe capture arg
        if (str.length() > 1 && str[0] == '-') return false;

        return true;
    }



    OptionType getOptionType(const std::string_view ref) {
        if (ref.length() > 2 && ref[0] == '-' && ref[1] == '-') return OptionType::Option;
        if (ref.length() > 2 && ref[0] == '-' && ref[1] != '-') return OptionType::MultiFlag;
        if (ref.length() > 1 && ref[0] == '-' && !(ref[0] >= 48 && ref[0] <= 57)) return OptionType::Flag;

        return OptionType::Positional;
    }


    struct opcontext {
        std::size_t pos = 0;
        std::string_view name{};
        std::string_view ctx{};
        OptionType opt_type;
        arg_iter_t& curr;
        arg_iter_t end;
        opcontext(arg_iter_t& s, arg_iter_t e) :curr(s), end(e) {}
        bool invalid() const { return curr == end; }
    };

	template<CompareType Cmp = CompareType::ByName, typename Tuple>
	bool applicator(Tuple& opts, opcontext& ctx) {
		return std::apply([&](auto& ...item) {
			if constexpr (Cmp == CompareType::ByPosition) {
				return (((item.position == ctx.pos) ? item.consume(ctx) : false) || ...);
			}
			else if constexpr (Cmp == CompareType::ByContext) {
				return (((item.name == ctx.ctx) ? item.consume(ctx) : false) || ...);
			}
			else if constexpr (Cmp == CompareType::ByName) {
				return (((item.name == ctx.name) ? item.consume(ctx) : false) || ...);
			}
			}, opts);
	}


    template<typename T>
    bool in_list(std::vector<T>& list, T& name) {
        return std::find(std::begin(list), std::end(list), name) != std::end(list);
    }

    bool in_list_view(std::vector<std::string>& list, std::string_view& name) {
        return std::find(std::begin(list), std::end(list), name) != std::end(list);
    }




    template<typename T>
    struct Converter {

        T operator()(const std::string& item) {
            static_assert("Undefined type conversion");
        }
    };

    template<>
    struct Converter<int> {
        int operator()(const std::string& item) {
            return std::stol(item);
        }
    };

    template<>
    struct Converter<float> {
        float operator()(const std::string& item) {
            return std::stof(item);
        }
    };

    template<>
    struct Converter<std::string> {
        std::string operator()(const std::string& item) {
            return item;
        }
    };



    template<int Pos = -1, typename ...Opts>
    struct Command {
        std::tuple<Opts...> value;
        using result_ot = decltype(value);
        static constexpr int position{ Pos };

        int captured_position = -1;
        std::string name;

        Command(const std::string_view& n, const Opts& ...oargs)
            :name(n), value(std::forward_as_tuple(oargs...)) {};

        bool is_found() const { return captured_position != -1; }
        bool consume(opcontext& ctx) {
            if (ctx.invalid()) return false;
            if (ctx.opt_type == OptionType::Positional) {
                return applicator<CompareType::ByPosition>(value, ctx);
            }
            else {
                return applicator<CompareType::ByName>(value, ctx);
            }
        }
    };

    template<int Pos = -1, typename ...Opts>
    Command<Pos, Opts...>
        make_command(const std::string_view& n, const Opts& ...oargs)
    {
        return { n,oargs... };
    }


    template<typename>
    struct is_command : std::false_type {};

    template<int Pos, typename ...Opts>
    struct is_command<Command<Pos, Opts...>> : std::true_type {};



    struct DefaultHelpFormatter {

        template<typename Option>
        void operator()(const Option& opt, std::stringstream& out) {
            if constexpr (is_command<Option>()) {
                //Print help for the command
                out << opt.name << '\n';
                std::apply([this, &out](auto&& ...item) {
                    out << '\t';
                    (operator()(item, out), ...);
                    }, opt.value);
            }
            else {
                out << opt.name << '\n' << '\t' << opt.description << '\n';
            }
        }

    };

    struct DefaultMissingFormatter {

        template<typename Option>
        bool operator()(const Option& opt, std::stringstream& out) {
            if constexpr (is_command<Option>()) {
                //Print help for the command
                std::apply([this, &out](auto&& ...item) {
                    (operator()(item, out), ...);
                    }, opt.value);
            }
            else {
                if (!opt.is_found() && opt.req) {
                    out << opt.name << ", ";
                    return true;
                }
            }
            return false;
        }

    };


    struct DefaultErrorHandler {
        template<typename Exception>
        void operator()(std::size_t pos, std::vector<std::string>& passed_args, Exception except) {

            std::stringstream error_preview;
            std::size_t count = 0;
            std::size_t carret_pos = 0;

            error_preview << "Error Occurred: " << except.what() << '\n';

            for (std::string item : passed_args) {
                error_preview << item << ' ';

                if (count <= pos) {
                    carret_pos += item.length();
                }
                ++count;
            }

            std::size_t carret = 40 - passed_args[pos].length();
            int start = 0;
            if (carret_pos > carret) {
                start = carret_pos - carret;
            }
            else {
                carret = carret_pos;
            }

            std::cout << '\n';
            std::cout << error_preview.str().substr(start, 80) << '\n';
            std::cout << std::string(carret - 1, ' ') << '^' << " Error occured here\n\n";

        }
    };

    struct option_exception :std::runtime_error {

        option_exception(const std::string& name, const std::string& err) :
            std::runtime_error(err + " @ " + name) {};
    };

    struct option_context_exception :std::runtime_error {

        option_context_exception(const std::string& name, const std::string& context, const std::string& err) :
            std::runtime_error(err + " @ " + name + " in " + context) {};
    };


    /*
        Types of parameters that we can capture
    */

    template<typename T, int Pos = -1>
    struct Option { //can we use it internally and ditch the inheritance because its aids

        using result_t = T;
        using result_ot = std::optional<T>;

        static constexpr int position{ Pos };
        int captured_position = -1;

        bool req = false;
        std::string name;
        std::string description;
        std::string help;
        short pos{ -1 };
        std::optional<result_t> value{};
        std::vector<T> possible_values;

        Option& required() { req = true; return *this; };
        Option& default_value(const result_t& val) { value = val; return *this; };
        Option& help_text(const std::string& help_txt) { help = help_txt; return *this; };
        Option& desc(const std::string& desc) { description = desc; return *this; };
        Option& posit(short p) { pos = p; return *this; };
        Option& choice(std::initializer_list<T> list) {
            possible_values = list;
            return *this;
        }

        Option(const std::string& n) :name(n) {};

        bool is_found() const { return captured_position != -1; }
        bool consume(opcontext& ctx) {

            if (ctx.invalid()) return false;
            auto temp = Converter<T>()(*ctx.curr);

            if (!possible_values.empty() && !in_list(possible_values, temp))
                throw option_exception(name, "Value must be any of X");

            if (position != -1 && ctx.pos != position)
                throw option_exception(name, "Invalid capture position");

            value = temp;
            captured_position = ctx.pos;
            ++ctx.curr;

            return true;
        }

    };

    using Flag = Option<bool>;


    template<typename T, int Pos, std::size_t Min, std::size_t Max = 255>
    struct List {

        using result_t = std::vector<T>;
        using result_ot = std::optional<result_t>;

        int captured_position = -1;

        static constexpr int position{ Pos };
        result_t value;
        bool req = false;
        std::string name;
        std::string description{}; //maybe another way to reform this
        std::string help_text{};

        List(const std::string& n) :name(n) {};
        List& help(const std::string& help_txt) { help_text = help_txt; return *this; };
        List& desc(const std::string& desc) { description = desc; return *this; };

        bool is_found() const { return captured_position != -1; }

        bool consume(opcontext& ctx) {
            std::size_t cnt = 0;
            while (!ctx.invalid() && cnt <= Max) {

                if (!validCapture(*ctx.curr)) {
                    --ctx.curr;
                    break;
                }

                auto temp = Converter<T>()(*ctx.curr);
                if (position != -1 && ctx.pos != position)
                    throw option_exception(name, "Invalid capture position");

                value.push_back(temp);
                if (captured_position == -1)
                    captured_position = ctx.pos;

                ++ctx.curr;
                ++cnt;
            }
            return cnt >= Min;
        }
    };





    template<typename T, template<typename> typename Filter>
    constexpr auto pluck() {
        return std::get<Filter<std::decay<T>::type>::value ? 0u : 1u>(
            std::make_tuple(
                [](T&& arg) { return std::tuple<T>{ std::forward<T&&>(arg)}; },
                [](T&&) { return std::tuple<>{}; })
            );
    };

    template<template<typename> typename Filter, typename ...Args>
    constexpr auto filter_tuple_elements(Args&& ...args) {
        return std::tuple_cat(pluck<Args, Filter>()(std::forward<Args&&>(args))...);
    };


    /*
        Main functions
    */

    template<typename ErrorHandler = DefaultErrorHandler,
        typename ...Arg >
        bool parse(int argc, const char* argv[], Arg&... args) {

        using opt_types = typename filtered_types<pred<is_command>::neg, Arg...>::type;
        using arg_var = typename unique_variant<opt_types>::type;
        using arg_cmd_tupl = typename filtered_types<is_command, Arg &...>::type::tuple_t;
        using arg_opt_tupl = typename filtered_types<pred<is_command>::neg, Arg &...>::type::tuple_t;

        //Commands
        auto cmds = filter_tuple_elements<is_command>(args...);
        //Opts
        auto opts = filter_tuple_elements<pred<is_command>::neg>(args...);



        std::vector<std::string> command_names =
            std::apply([](auto&& ...item) {
            return std::vector<std::string>{item.name...};
                }, cmds);


        std::vector<std::string> passed_args(argv + 1, argv + argc);

        std::size_t pos = 0;
        auto starting_pos = std::begin(passed_args);
        auto end = std::end(passed_args);

        bool in_options;
        bool in_context;
        auto curr = starting_pos;

        opcontext ctx(curr, end);
        try {

            while (ctx.curr != end) {
                in_options = false;
                in_context = false;


                ctx.name = *curr;

                ctx.opt_type = getOptionType(*ctx.curr);
                pos = ctx.pos = std::distance(starting_pos, ctx.curr);
                if (ctx.opt_type != OptionType::Positional) {
                    ++ctx.curr;
                }


                if (ctx.opt_type == OptionType::Positional &&
                    in_list_view(command_names, ctx.name)) {

                    ctx.ctx = ctx.name;
                    ++ctx.curr;
                    continue;
                }

                if (ctx.opt_type == OptionType::Positional) {
                    in_options = applicator<CompareType::ByPosition>(opts, ctx);
                }
                else {
                    in_options = applicator<CompareType::ByName>(opts, ctx);
                }

                if (in_options) continue;

                //Check if we are into context
                if (!in_options || !ctx.ctx.empty()) {
                    in_context = applicator<CompareType::ByContext>(cmds, ctx);

                    if (!in_context) {
                        throw option_context_exception(ctx.name.data(), ctx.ctx.data(), "Invalid option detected in context");
                    }

                }
            }

            //Test if we had all the requred options captured
            std::stringstream missing;

            bool failed = std::apply([&missing](auto&& ...per) {
                auto miss = DefaultMissingFormatter{};
                return (miss(per, missing) || ...);
                }, std::make_tuple(args...));
            if (!failed)
                std::cout << "Missing args:\n" << missing.str() << '\n';

            return !failed;
        }
        catch (const std::exception & e) {

            ErrorHandler{}(pos, passed_args, e);
            return false;
        }

    };





    template<typename Formatter = DefaultHelpFormatter,
        typename ...Args>
        void help(const Args& ... opts) {
        std::stringstream output;

        std::apply([&output](auto&& ...item) {
            auto format = Formatter{};
            (format(item, output), ...);
            }, std::make_tuple(opts...)); //should work recursively ? no ?

        std::cout << output.str() << '\n';
    };

}



#endif // !TUPLE_OF_ARGS