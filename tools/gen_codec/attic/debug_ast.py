#!/usr/bin/env python3

from lark import Lark
from gen_codec_fixed import PDL_GRAMMAR, PDLTransformer

# Test basic parsing
content = '''def u16_le = { type: u16 }'''

parser = Lark(PDL_GRAMMAR, parser='lalr')
transformer = PDLTransformer()

parse_tree = parser.parse(content)
print("Parse tree:", parse_tree.pretty())

ast = transformer.transform(parse_tree)
print("AST:", ast)
print("AST type:", type(ast))