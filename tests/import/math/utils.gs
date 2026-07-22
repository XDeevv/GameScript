namespace math/utils;

::print("-> math/utils.gs is compiling and executing!\n");

func hidden_multiplier(val) {
    return val * 2;
}

pub func calculate(a) {
    return hidden_multiplier(a);
}