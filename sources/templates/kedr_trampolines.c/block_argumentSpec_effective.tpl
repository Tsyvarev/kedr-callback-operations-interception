<$if function.args$><$for arg in function.args$><$if not loop.first$>, <$endif$><$include 'block_arg'$><$endfor$><$if function.ellipsis$>, va_list args<$endif$><$else$>void<$endif$>