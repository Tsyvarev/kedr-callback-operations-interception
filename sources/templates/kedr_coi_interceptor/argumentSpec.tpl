<$if operation.args$><$for arg in operation.args$><$if not loop.first$>, <$endif$>{{arg.type}} {{arg.name}}<$endfor$><$else$>void<$endif$>
