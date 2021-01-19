# Tuple of Args
Just a couple of arguments, you know, in a `std::tuple`.

Single header parsing library, more of an exploration of the language than a real world use library, but with the hopes of someone getting inspired and maybe contributing to the cause :)

# Types of options

## Option
Each option has a type and a position as template arguments `Option<type,pos>`
and the following chainable member functions

```cpp
Option& required()
Option& default_value(const result_t& val)
Option& help_text(const std::string& help_txt)
Option& desc(const std::string& desc)
Option& choice(std::initializer_list<T> list)
```

## List
Similar to Option but captures multiple items at once. List also has a type and positional type arguments, but features two more: a min and maximum number of arguments; `List<float,-1,1,5>("--floats")` will capture at any position at least 1, but a maximum of 5 floats

## Command
Simple wrapper for creating command line subcommands like `git blame`, Its constructed using a name followed by option and list classes that are local to the subcommand.

```cpp
Command("blame",Option<int>("--uid"),Option<float>("--test"))
```


# Usage example
It the same one shown in the example folder, but most of the code is self explanatory

```cpp
sing namespace tuple_of_args;

    auto commit = Command("commit", 
        Option<int>("--test"), 
        Option<float>("--ding"));
    auto push = Command("push", 
        Option<float>("--test"), 
        Option<float>("--dong"));
    auto paths = List<std::string, -1, 2>("--paths");
    auto num = Option<int,1>("num")
        .desc("This is great")
        .default_value(3)
        .choice({ 3,12,24 });
    /*auto num = Option<int>("--num")
        .desc("This is great")
        .default_value(3);*/
    auto fl = Option<float>("--fl")
        .desc("This is great")
        .default_value(5.14f);
    auto flz = Option<float>("--flz")
        .desc("This is not great")
        .default_value(5.14f);


    
    bool parsed = parse(argc, argv, commit, push, paths, num, fl, flz);
    
    if(!parsed) 
        help(commit, push, paths, num, fl, flz);

    if (num.value == 12) {
        std::cout << "Found the value";
    }
```