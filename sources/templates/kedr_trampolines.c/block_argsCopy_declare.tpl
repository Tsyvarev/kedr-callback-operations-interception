<$if function.args | join(attribute='name')$>        <$if function.ellipsis$>va_list args_copy;
        <$endif$><$if args_copy_declare_and_init$>{{args_copy_declare_and_init}}
        <$else$><$for arg in function.args$><$if not loop.first$>

        <$endif$><$include 'block_argCopy_declare'$><$endfor$>

        <$endif$><$if function.ellipsis$>va_start(args_copy, {{function.last_arg}});
<$endif$><$endif$>