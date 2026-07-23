func assert_equal<T>(a: T, b: T) -> bool {
    return a == b;
}

println(assert_equal(100, 100)); // Should pass (true)
println(assert_equal("test", "tessst")); // Should pass (true)

// UNCOMMENT TO TEST:
println(assert_equal(42, "forty-two"));