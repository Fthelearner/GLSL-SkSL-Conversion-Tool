# SkSL <-> glslang AST Mapping

## 1. Comparison baseline

- SkSL side in this document means the semantic IR produced after `Parser` and `*::Convert`
  have finished. The relevant sources are:
  - `skia/src/sksl/SkSLParser.cpp`
  - `skia/src/sksl/ir/SkSLIRNode.h`
  - `skia/src/sksl/ir/*`
- glslang side means the front-end `TInterm*` tree produced by:
  - `glslang/glslang/MachineIndependent/glslang.y`
  - `glslang/glslang/MachineIndependent/ParseHelper.cpp`
  - `glslang/glslang/MachineIndependent/Intermediate.cpp`
  - `glslang/glslang/Include/intermediate.h`
- This is a mapping of actual AST/IR objects, not only a mapping of source-language grammar.

## 2. Non-bijective hotspots

These are the most important differences to remember before implementing any conversion:

1. SkSL lowers directly into typed semantic IR.
   A single source form often gets its own C++ class, such as `SwitchCase`,
   `ConstructorDiagonalMatrix`, or `FunctionPrototype`.

2. glslang uses fewer node classes.
   Much of the syntax is encoded as:
   - node class: `TIntermBinary`, `TIntermUnary`, `TIntermAggregate`, `TIntermSelection`,
     `TIntermLoop`, `TIntermBranch`, `TIntermVariableDecl`
   - operator tag: `TOperator`
   - context: parent node, result type, and parse-time side effects

3. SkSL and glslang do not preserve the same amount of syntax sugar.
   Examples:
   - SkSL `while (...)` is lowered to `ForStatement::ConvertWhile`, so there is no dedicated
     `WhileStatement` node.
   - glslang `for (...)` is not a single node. It is usually a parent `EOpSequence`/`EOpScope`
     whose children are `initializer` and `TIntermLoop`.
   - SkSL splits constructor shapes into many subclasses. glslang mostly collapses them into
     constructor operators.

4. Some top-level constructs exist only as parser state on the glslang side.
   Examples:
   - function prototypes
   - `precision` statements
   - standalone qualifier/default declarations
   - struct definitions
   - extension directives

5. glslang may drop declaration nodes unless debug info is enabled.
   `declareVariable()` returns `TIntermVariableDecl` only when
   `intermediate.getDebugInfo()` is true. Otherwise it often returns:
   - an initializer assignment
   - or `nullptr` for const/uniform/null-init cases
   This is the single biggest obstacle to exact source-level round-tripping from glslang AST.

## 3. Mapping legend

- `1:1`: stable direct mapping
- `1:N`: one SkSL node maps to multiple glslang node patterns
- `N:1`: multiple SkSL nodes collapse into one glslang form
- `none`: no real counterpart on the other side

## 4. Top-level / program-element mapping

| SkSL IR / syntax | glslang form | Class | Notes |
| --- | --- | --- | --- |
| `Program` root | `translation_unit`, usually rooted at `TIntermAggregate(EOpSequence)` | `1:1` | glslang root may contain `nullptr` entries for declarations that only affect parser state. |
| `Extension` | no stable `TInterm*` node | `none` | `#extension` is handled before AST construction. SkSL keeps it as `ProgramElement`; glslang mostly keeps only side effects. |
| `FunctionDefinition` | `TIntermAggregate(EOpFunction)` | `1:1` | The aggregate name is the mangled function name. Children are usually parameter aggregate plus body aggregate. |
| `FunctionPrototype` | no final AST node | `none` | glslang handles prototypes in symbol tables only; `declaration -> function_prototype ';'` returns `0`. |
| `GlobalVarDeclaration` | `TIntermVariableDecl`, or assignment, or `nullptr` | `1:N` | Exact declaration round-trip requires debug-info declaration nodes. |
| `InterfaceBlock` | symbol-table block type plus optional debug-only declaration sequence | `1:N` | glslang builds the block type and variable, but does not normally keep a dedicated interface-block AST node. |
| `ModifiersDeclaration` | no final AST node | `none` | Maps to `type_qualifier ';'`, `precision ... ;`, or default-qualifier updates in glslang parser state. |
| `StructDefinition` | no final AST node | `none` | glslang stores struct type info in `TType`/symbol table, not as a top-level `TInterm*` node. |

### 4.1 Practical consequence

If you need a reversible top-level mapping, glslang AST alone is not sufficient. You will need at
least one of:

- debug declaration nodes enabled
- captured parser events
- symbol-table snapshots
- a side-band syntax record for prototypes, precision/default qualifiers, struct definitions, and
  extensions

## 5. Statement mapping

### 5.1 Source-syntax normalization

SkSL and glslang normalize loop syntax differently:

- SkSL source `while (...) stmt` -> `ForStatement` with `initializer == null` and `next == null`
- SkSL source `do stmt while (...)` -> dedicated `DoStatement`
- glslang source `while`, `for`, and `do-while` -> `TIntermLoop` plus surrounding context

### 5.2 Node mapping

| SkSL statement | glslang form | Class | Notes |
| --- | --- | --- | --- |
| `Block` | `TIntermAggregate(EOpSequence)` or `TIntermAggregate(EOpScope)` | `1:1` | Reverse mapping must inspect context to decide `kBracedScope`, `kUnbracedBlock`, or `kCompoundStatement`. |
| `BreakStatement` | `TIntermBranch(EOpBreak)` | `1:1` | Direct mapping. |
| `ContinueStatement` | `TIntermBranch(EOpContinue)` | `1:1` | Direct mapping. |
| `DiscardStatement` | `TIntermBranch(EOpKill)` | `1:1` | glslang also has fragment-only `EOpDemote` and `EOpTerminateInvocation`, which have no SkSL equivalent. |
| `DoStatement` | `TIntermLoop(testFirst = false, terminal = null)` | `1:1` | Direct if loop is not wrapped in a preceding initializer sequence. |
| `ExpressionStatement` | the expression node itself, or `nullptr` for bare `;` | `1:N` | glslang has no expression-statement wrapper node. |
| `ForStatement` | parent `EOpSequence`/`EOpScope` with `initializer`, then `TIntermLoop(testFirst = true, terminal maybe non-null)` | `1:N` | Reverse mapping must look at both the `TIntermLoop` and its parent aggregate. |
| `IfStatement` | `TIntermSelection` with result type `void` | `1:1` | `trueBlock` and `falseBlock` are statement nodes, not typed expressions. |
| `Nop` | `nullptr` | `1:1` | Empty statement or empty expression statement becomes no node on the glslang side. |
| `ReturnStatement` | `TIntermBranch(EOpReturn, expr?)` | `1:1` | Direct mapping. |
| `SwitchStatement` | `TIntermSwitch` | `1:1` | Body representation differs; see `SwitchCase` row. |
| `SwitchCase` | `TIntermBranch(EOpCase/EOpDefault)` plus following statement sequence in switch body | `1:N` | glslang has no dedicated switch-case node class. Case label and case body are split. |
| `VarDeclaration` | `TIntermVariableDecl`, or assignment, or `nullptr` | `1:N` | Same declaration-node caveat as globals. Multiple declarators become sequences on both sides. |

### 5.3 Reverse rules for loops

To reconstruct SkSL loop syntax from glslang:

1. If you see `TIntermLoop(testFirst = false, terminal = null)`, map to `DoStatement`.
2. If you see `TIntermLoop(testFirst = true, terminal = null)` with no preceding initializer node
   in the same parent sequence, map to SkSL `while`, which is represented as `ForStatement`.
3. If you see a parent aggregate whose children are:
   - initializer node
   - `TIntermLoop(testFirst = true, terminal maybe non-null)`
   then map to SkSL `ForStatement`.
4. glslang `conditionopt` missing test is represented as `test == null`.
   SkSL can represent this directly in `ForStatement`.

### 5.4 Reverse rules for switch cases

Inside a glslang `TIntermSwitch` body:

- `TIntermBranch(EOpCase, expr)` starts a new case
- `TIntermBranch(EOpDefault)` starts a default case
- the following one sequence node, if present, is the case body

To rebuild SkSL:

1. scan the switch body sequence in order
2. pair each `EOpCase`/`EOpDefault` with the following subsequence
3. wrap the body subsequence as the `SwitchCase.statement()`
4. accumulate all `SwitchCase` nodes into the `SwitchStatement` case block

## 6. Expression mapping

### 6.1 Primary, reference, access, and call expressions

| SkSL expression | glslang form | Class | Notes |
| --- | --- | --- | --- |
| `Literal` | `TIntermConstantUnion` | `1:1` | Direct mapping for bool/int/float and related literal categories. |
| `VariableReference` | `TIntermSymbol` | `1:1` | Direct mapping. Write/read-write intent in SkSL must be reconstructed from surrounding assignments and out-parameter calls. |
| `FunctionReference` | no final AST node | `none` | glslang resolves function identifiers into `TFunction` metadata before final call construction. |
| `TypeReference` | no final AST node | `none` | glslang type names are grammar-level `type_specifier`, not persistent expression nodes. |
| `FieldAccess` | `TIntermBinary(EOpIndexDirectStruct)` | `1:1` | Only for real struct/interface members. SkSL also uses `FieldAccess::Convert` to branch into `Setting` and `MethodReference`, which do not map this way. |
| `IndexExpression` | `TIntermBinary(EOpIndexDirect)` or `TIntermBinary(EOpIndexIndirect)` | `1:1` | Struct-member indexing is a field access on the glslang side, not a general index node. |
| `Swizzle` | `TIntermBinary(EOpVectorSwizzle)` or sometimes direct constant index on vectors | `1:N` | SkSL canonicalizes constant vector indexing to swizzle. glslang may keep `EOpIndexDirect` or explicit swizzle op. |
| `FunctionCall` | `TIntermAggregate(EOpFunctionCall)` or builtin operator node(s) | `1:N` | glslang builtin calls may become unary/binary/aggregate operator nodes instead of generic call nodes. |
| `MethodReference` | superficially nearest to `TIntermMethod`, but not semantically equivalent | `none` | SkSL method references are effect-child call placeholders. glslang `TIntermMethod` is an unresolved parser helper, mostly for `.length()`. |
| `ChildCall` | no glslang counterpart | `none` | SkSL-only effect system concept. |
| `Setting` | no glslang counterpart | `none` | SkSL-only `sk_Caps.*` compile-time setting. |
| `Poison` | no direct counterpart | `none` | SkSL error-sentinel node. glslang mostly uses parser recovery with null/dummy constructs. |
| `EmptyExpression` | no direct expression node | `none` | Missing glslang `for` test/update is represented as `nullptr`, not as a typed void expression. |

### 6.2 Operator and control expressions

| SkSL expression | glslang form | Class | Notes |
| --- | --- | --- | --- |
| `BinaryExpression` | mostly `TIntermBinary`; comma uses `TIntermAggregate(EOpComma)` | `1:N` | `*`, `*=`, `==`, `!=`, and `!` split by operand shape on the glslang side. See Section 7. |
| `PrefixExpression` | `TIntermUnary` | `1:1` | Includes pre-inc, pre-dec, unary minus, logical not, bitwise not. |
| `PostfixExpression` | `TIntermUnary(EOpPostIncrement/EOpPostDecrement)` | `1:1` | Direct mapping. |
| `TernaryExpression` | `TIntermSelection` with non-void result type | `1:1` | Reverse rule: `TIntermSelection` with non-void result is ternary, with void result is `IfStatement`. |

### 6.3 Constructor family

SkSL has a much richer constructor taxonomy than glslang. Most of these collapse into one of:

- `TIntermAggregate(EOpConstruct*)`
- `TIntermAggregate(EOpConstructStruct)`
- `TIntermUnary` numeric conversion

| SkSL constructor node | glslang form | Class | Notes |
| --- | --- | --- | --- |
| `ConstructorArray` | constructor aggregate, array-specific handling in `addConstructor()` / `constructAggregate()` | `N:1` | glslang does not keep a dedicated array-constructor node class. |
| `ConstructorArrayCast` | same constructor aggregate plus element conversions | `N:1` | No direct glslang node distinguishes cast-vs-non-cast array construction. |
| `ConstructorCompound` | `TIntermAggregate(EOpConstructVec* / EOpConstructMat* / scalar construct)` | `N:1` | Reverse conversion depends on target type and argument shape. |
| `ConstructorCompoundCast` | same aggregate form | `N:1` | Must be inferred from target type and child conversions. |
| `ConstructorDiagonalMatrix` | same aggregate form | `N:1` | SkSL splits out the one-scalar-to-diagonal-matrix case; glslang does not. |
| `ConstructorMatrixResize` | same aggregate form | `N:1` | SkSL separates matrix-resize semantics; glslang keeps generic constructor semantics. |
| `ConstructorScalarCast` | `TIntermUnary` conversion or scalar constructor op | `N:1` | Use result type and source type to decide whether this is a cast or a generic constructor when rebuilding SkSL. |
| `ConstructorSplat` | same aggregate form | `N:1` | SkSL keeps the scalar-splat case explicit; glslang usually does not. |
| `ConstructorStruct` | `TIntermAggregate(EOpConstructStruct)` | `1:1` | This is the cleanest constructor mapping. |

### 6.4 Reverse rule for constructors

When converting glslang constructors back to SkSL, choose the SkSL constructor subclass by:

1. target type category:
   - scalar
   - vector
   - matrix
   - array
   - struct
2. argument count
3. argument shape:
   - single scalar
   - single matrix
   - single same-category value
   - flattened scalar/vector list
4. whether per-element coercion is required

A good canonical policy is:

- scalar target + one argument + numeric domain change -> `ConstructorScalarCast`
- vector or matrix target + one scalar argument -> `ConstructorSplat` or `ConstructorDiagonalMatrix`
- matrix target + one matrix argument of different dimensions -> `ConstructorMatrixResize`
- same-category compound target + per-element coercion -> `ConstructorCompoundCast`
- struct target -> `ConstructorStruct`
- array target -> `ConstructorArray` or `ConstructorArrayCast`
- otherwise -> `ConstructorCompound`

## 7. Operator mapping

glslang splits some operators more aggressively than SkSL. This section is the operator-level key
needed to convert `BinaryExpression`, `PrefixExpression`, and `PostfixExpression`.

### 7.1 Binary and assignment operators

| SkSL operator | glslang operator(s) | Notes |
| --- | --- | --- |
| `PLUS` | `EOpAdd` | Direct arithmetic add. |
| `MINUS` | `EOpSub` | Direct arithmetic subtract. |
| `STAR` | `EOpMul`, `EOpVectorTimesScalar`, `EOpVectorTimesMatrix`, `EOpMatrixTimesVector`, `EOpMatrixTimesScalar`, `EOpMatrixTimesMatrix` | SkSL keeps one `*`; glslang encodes operand-shape semantics in `TOperator`. |
| `SLASH` | `EOpDiv` | Direct divide. |
| `PERCENT` | `EOpMod` | Direct modulo. |
| `SHL` | `EOpLeftShift` | Direct shift. |
| `SHR` | `EOpRightShift` | Direct shift. |
| `BITWISEAND` | `EOpAnd` | Direct bitwise and. |
| `BITWISEOR` | `EOpInclusiveOr` | Direct bitwise or. |
| `BITWISEXOR` | `EOpExclusiveOr` | Direct bitwise xor. |
| `LOGICALAND` | `EOpLogicalAnd` | Direct logical and. |
| `LOGICALOR` | `EOpLogicalOr` | Direct logical or. |
| `LOGICALXOR` | `EOpLogicalXor` | Direct logical xor. |
| `EQEQ` | `EOpEqual`, `EOpVectorEqual` | glslang splits scalar vs vector equality. |
| `NEQ` | `EOpNotEqual`, `EOpVectorNotEqual` | glslang splits scalar vs vector inequality. |
| `LT` | `EOpLessThan` | Direct relational compare. |
| `GT` | `EOpGreaterThan` | Direct relational compare. |
| `LTEQ` | `EOpLessThanEqual` | Direct relational compare. |
| `GTEQ` | `EOpGreaterThanEqual` | Direct relational compare. |
| `EQ` | `EOpAssign` | Direct assignment. |
| `PLUSEQ` | `EOpAddAssign` | Direct compound assignment. |
| `MINUSEQ` | `EOpSubAssign` | Direct compound assignment. |
| `STAREQ` | `EOpMulAssign`, `EOpVectorTimesMatrixAssign`, `EOpVectorTimesScalarAssign`, `EOpMatrixTimesScalarAssign`, `EOpMatrixTimesMatrixAssign` | Split by operand shape. |
| `SLASHEQ` | `EOpDivAssign` | Direct compound assignment. |
| `PERCENTEQ` | `EOpModAssign` | Direct compound assignment. |
| `SHLEQ` | `EOpLeftShiftAssign` | Direct compound assignment. |
| `SHREQ` | `EOpRightShiftAssign` | Direct compound assignment. |
| `BITWISEANDEQ` | `EOpAndAssign` | Direct compound assignment. |
| `BITWISEOREQ` | `EOpInclusiveOrAssign` | Direct compound assignment. |
| `BITWISEXOREQ` | `EOpExclusiveOrAssign` | Direct compound assignment. |
| `COMMA` | `TIntermAggregate(EOpComma)` | glslang does not model comma as `TIntermBinary`. |

### 7.2 Unary operators

| SkSL operator / node | glslang operator(s) | Notes |
| --- | --- | --- |
| prefix `PLUSPLUS` | `EOpPreIncrement` | Direct mapping. |
| prefix `MINUSMINUS` | `EOpPreDecrement` | Direct mapping. |
| postfix `PLUSPLUS` | `EOpPostIncrement` | Direct mapping. |
| postfix `MINUSMINUS` | `EOpPostDecrement` | Direct mapping. |
| unary `MINUS` | `EOpNegative` | Direct mapping. |
| unary `LOGICALNOT` | `EOpLogicalNot`, `EOpVectorLogicalNot` | glslang may split scalar vs vector logical not. |
| unary `BITWISENOT` | `EOpBitwiseNot` | Direct mapping. |

## 8. glslang-only forms that need explicit policy

These glslang AST forms do not have a clean dedicated SkSL node:

- `TIntermAggregate(EOpParameters)`
  - SkSL stores parameters on `FunctionDeclaration`, not as a separate AST node.
- `TIntermMethod`
  - parser helper for unresolved method syntax, especially `.length()`
  - not a stable counterpart of SkSL `MethodReference`
- `TIntermBranch(EOpCase/EOpDefault)`
  - need to be folded into `SwitchCase`
- `TIntermAggregate(EOpScope)`
  - debug/scoping aggregate; usually maps to `Block`
- `TIntermBranch(EOpDemote)`, `EOpTerminateInvocation`, `EOpTerminateRayKHR`,
  `EOpIgnoreIntersectionKHR`
  - no direct SkSL statement equivalent
- builtin-function operators such as `EOpSin`, `EOpTexture*`, `EOpImage*`, `EOpSparse*`
  - often need to be reconstructed as canonical intrinsic `FunctionCall` on the SkSL side
  - some have no SkSL equivalent at all

## 9. SkSL-only forms that need explicit policy

These SkSL nodes do not have a clean glslang counterpart:

- `Extension`
- `ModifiersDeclaration`
- `StructDefinition`
- `FunctionPrototype`
- `Setting`
- `ChildCall`
- `FunctionReference`
- `TypeReference`
- `Poison`
- `EmptyExpression`

For exact bidirectional conversion, these require side-band syntax preservation or a canonical
lossy lowering policy.

## 10. Recommended conversion pipeline

To make the two trees convertible in a controlled way, use a canonical intermediate model with the
following rules:

1. Separate syntax identity from semantic identity.
   Track:
   - declaration vs assignment
   - block kind
   - loop flavor
   - constructor flavor
   - call flavor: user call, builtin operator call, constructor call, child-effect call

2. Treat glslang aggregate nodes as context-sensitive containers.
   The same `TIntermAggregate` class can mean:
   - translation-unit sequence
   - block body
   - comma expression
   - function definition
   - parameter list
   - constructor call
   - builtin call

3. Rebuild missing SkSL distinctions explicitly.
   In particular:
   - `while` vs `for`
   - `SwitchCase`
   - constructor subclass
   - `ExpressionStatement` wrapper
   - `kCompoundStatement` vs normal block

4. Accept that some mappings are inherently lossy unless extra parse metadata is preserved.
   The minimum lossy cases are:
   - glslang prototypes, precision/default qualifiers, struct definitions, extensions
   - glslang declarations when debug declaration nodes are disabled
   - SkSL settings and child-effect calls

## 11. Source anchors

Use these anchors when implementing or verifying the mapper:

- SkSL node enums:
  - `skia/src/sksl/ir/SkSLIRNode.h`
- SkSL parser entry points:
  - `Parser::declaration`
  - `Parser::statement`
  - `Parser::expression`
  - `Parser::whileStatement`
  - `Parser::forStatement`
  - `Parser::switchStatement`
- SkSL call/reference construction:
  - `Symbol::instantiate`
  - `FunctionCall::Convert`
  - `FieldAccess::Convert`
  - `IndexExpression::Convert`
  - `Swizzle::Convert`
- glslang grammar:
  - `primary_expression`
  - `postfix_expression`
  - `function_call`
  - `declaration`
  - `statement`
  - `selection_statement`
  - `switch_statement`
  - `iteration_statement`
  - `jump_statement`
  - `function_definition`
- glslang AST node definitions:
  - `TIntermVariableDecl`
  - `TIntermSymbol`
  - `TIntermConstantUnion`
  - `TIntermUnary`
  - `TIntermBinary`
  - `TIntermAggregate`
  - `TIntermSelection`
  - `TIntermLoop`
  - `TIntermBranch`
  - `TIntermSwitch`
- glslang AST builders:
  - `TParseContext::declareVariable`
  - `TParseContext::declareBlock`
  - `TParseContext::handleFunctionDefinition`
  - `TParseContext::handleFunctionCall`
  - `TParseContext::addConstructor`
  - `TParseContext::addSwitch`
  - `TIntermediate::addLoop`
  - `TIntermediate::addSelection`
  - `TIntermediate::addComma`
  - `TIntermediate::addSwizzle`
  - `TIntermediate::setAggregateOperator`

## 12. Bottom line

The two front ends are not structurally symmetric:

- SkSL is class-rich and syntax-aware.
- glslang is operator-rich and context-aware.

So the correct way to build a converter is not "node-name to node-name", but:

1. normalize both sides into a canonical semantic model
2. preserve side-band syntax where the target AST requires distinctions the source AST drops
3. re-expand target-specific syntax only at the final emission step

If you follow that rule, the mapping above is complete enough to serve as the implementation
checklist for a first-pass SkSL <-> glslang AST converter.
