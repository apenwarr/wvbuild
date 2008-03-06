
#include "wvautoconf.h"
#ifdef WITH_POPT

#include "wvtest.h"
#include "wvargs.h"

WVTEST_MAIN("bools")
{
    bool bool_val = false;
   
    WvArgs args;

    args.add_set_bool_option('s', "set-bool", "Set the bool", bool_val);
    args.add_reset_bool_option('r', "reset-bool", "Reset the bool", bool_val);
    args.add_flip_bool_option('f', "flip-bool", "Flip the bool", bool_val);

    args.set_flag(WvArgs::NO_EXIT_ON_ERRORS, true);

    bool_val = false;
    char *av0[] = { "test" };
    WVPASS(args.process(1, av0));
    WVPASS(!bool_val);
    
    bool_val = false;
    char *av01[] = { "test", "--bad"};
    WVFAIL(args.process(2, av01));
    WVPASS(!bool_val);
    
    bool_val = false;
    char *av1[] = { "test", "-s" };
    WVPASS(args.process(2, av1));
    WVPASS(bool_val);
    
    bool_val = true;
    char *av2[] = { "test", "-r" };
    WVPASS(args.process(2, av2));
    WVPASS(!bool_val);
    
    bool_val = false;
    char *av3[] = { "test", "--set-bool" };
    WVPASS(args.process(2, av3));
    WVPASS(bool_val);
    
    bool_val = true;
    char *av21[] = { "test", "-f" };
    WVPASS(args.process(2, av21));
    WVPASS(!bool_val);
    
    bool_val = false;
    char *av31[] = { "test", "--flip-bool" };
    WVPASS(args.process(2, av31));
    WVPASS(bool_val);
    
    bool_val = true;
    char *av4[] = { "test", "--reset-bool" };
    WVPASS(args.process(2, av4));
    WVPASS(!bool_val);
    
    bool_val = false;
    char *av5[] = { "test", "-s", "--reset-bool" };
    WVPASS(args.process(3, av5));
    WVPASS(!bool_val);
}

template <typename T>
void numeric_test()
{
    T val;
    char arg[64];
    char *argv[] = { "test", arg };

    WvArgs args;

    args.add_option('s', "set", "Set the value", "The value to set", val);

    args.set_flag(WvArgs::NO_EXIT_ON_ERRORS, true);

    val = 1;
    WVPASS(args.process(1, argv));
    WVFAIL(val == 0);

    val = 1;
    sprintf(arg, "-s");
    WVFAIL(args.process(2, argv));
    WVFAIL(val == 0);

    val = 1;
    sprintf(arg, "-s-");
    WVFAIL(args.process(2, argv));
    WVFAIL(val == 0);

    val = 1;
    sprintf(arg, "-s0");
    WVPASS(args.process(2, argv));
    WVPASS(val == 0);

    val = 1;
    sprintf(arg, "--set");
    WVFAIL(args.process(2, argv));
    WVFAIL(val == 0);

    val = 1;
    sprintf(arg, "--set-");
    WVFAIL(args.process(2, argv));
    WVFAIL(val == 0);

    val = 1;
    sprintf(arg, "--set0");
    WVFAIL(args.process(2, argv));
    WVFAIL(val == 0);

    val = 1;
    sprintf(arg, "--set=0");
    WVPASS(args.process(2, argv));
    WVPASS(val == 0);
}

WVTEST_MAIN("numerics int")
{
    numeric_test<int>();
}

WVTEST_MAIN("numerics long")
{
    numeric_test<long>();
}

WVTEST_MAIN("numerics float")
{
    numeric_test<float>();
}

WVTEST_MAIN("numerics double")
{
    numeric_test<double>();
}

#endif // WITH_POPT
