contract c {
    function f1(mapping(uint => uint) calldata) pure external returns (mapping(uint => uint) memory) {}
}
// ----
// TypeError: (29-50): Type is required to live outside storage.
// TypeError: (29-50): Internal or recursive type is not allowed for public or external functions.
