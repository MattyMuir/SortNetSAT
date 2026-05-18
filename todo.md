# SortNetSAT TODO

## Working
- If shuffled outputs are slow, are sorted outputs fast?
- Fix/Ban sets of comparators, if UNSAT, add negation clause

## Complete
- Optimize prefix output generation
- Add expression sanity checking (dual/empty clauses, unused variables)
- phi and psi constraints
- Add an option to force network vertical symmetry
- Window size minimization
- Optimize window size minimization by permuting outputs not networks
- Optimize formula generator (remove strings, add variable families/tensors)
- Allow multiple symbolic variables to map to the same SAT variable
- Add fixed `true` and `false` variables