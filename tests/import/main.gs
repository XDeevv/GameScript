print("--- TESTING GENERICS & CUSTOM TYPES ---\n");

// Test 1: Function with generic type parameters and matching return type
func identity<T>(value: T) -> T {
    return value;
}

local res: int = identity<int>(100);
print("identity<int>(100) = " + res + "\n");

// Test 2: Void function validation
func log_data() -> void {
    print("Logging execution...\n");
    return;
}

log_data();
print("Void function executed successfully!\n");