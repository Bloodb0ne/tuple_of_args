#include "tuple_of_args.h"






int main(int argc, const char* argv[])
{


    using namespace tuple_of_args;

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
}