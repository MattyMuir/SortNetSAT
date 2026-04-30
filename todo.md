# SortNetSAT TODO

## Working
- Test best phi and psi combinations
- Optimize formula generator (remove strings, add variable families/tensors)
- Allow multiple symbolic variables to map to the same SAT variable
- Add fixed `true` and `false` variables

## Complete
- Optimize prefix output generation
- Add expression sanity checking (dual/empty clauses, unused variables)
- phi and psi constraints
- Add an option to force network vertical symmetry
- Window size minimization
- Optimize window size minimization by permuting outputs not networks