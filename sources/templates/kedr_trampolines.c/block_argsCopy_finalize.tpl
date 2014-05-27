<$if function.args | join(attribute='name')$><$if function.ellipsis$>        va_end(args_copy);
<$endif$><$if args_copy_destroy$>        {{args_copy_destroy}}
<$endif$><$endif$>