<$if operation.args | join(attribute='type')$><$for arg in operation.args$><$if not loop.first$>, <$endif$><$include 'block_arg'$><$endfor$><$else$>void<$endif$>