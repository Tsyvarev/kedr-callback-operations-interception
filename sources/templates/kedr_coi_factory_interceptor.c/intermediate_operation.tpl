    {
        .operation_offset = offsetof({{object.operations_type}}, {{operation.name}}),
        .repl = intermediate_operation_{{operation.name}},
    },