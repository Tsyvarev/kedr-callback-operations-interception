<$if operation.args | join(attribute='type')$>{{operation.args | join(d=", ", attribute="name")}}<$endif$>