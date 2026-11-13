# Bounded angular A/B compression evidence

Decision: **NO-GO for ranks 8/16/32 of the tested harmonic representation**.
See [method, budgets, results and reproduction](../../ibl-ab-compression-experiment.md).
No production shader, format, asset or GPU prototype is represented here.

- `inputs.txt`: 758 rows, N.xyz V.xyz Ng.xyz base.rgb roughness metallic AO,
  normalized directions in environment coordinates, effective shading normals.
- `rows.json`: matching row metadata, UUIDs, top-down pixels and orbit poses.
  `actual` values are captured **Studio production** values even when a candidate
  uses another lighting fixture; they are not oracle or candidate values.
- `*-reference.csv`: one-based row, low RGB, high RGB, maximum floor-relative
  refinement delta, convergence flag. All required rows are finite and converged.
  Methods/orders vary by group as documented; `axes-cross-checks.json` retains
  the independent source/NDF agreement for the final three grazing controls.
- `studio_8.csv` etc.: candidate linear specular RGB in identical input order.
  No clamping, exposure or display encoding is applied.
- `groups.csv`: compact group ledger. `metrics.json` adds channelwise errors,
  exact worst inputs/outputs, reference uncertainty, negative channel counts,
  interpolation controls and lighting projection convergence. Row IDs are
  one-based; channel indices are 0=R, 1=G, 2=B. Values are fractions, not percent.
- `table.csv`: FP32 transfer coefficients serialized with round-trip precision;
  dimensions 12 view nodes × 9 roughness nodes × 2 kernels × 36 computed cosine
  modes. A precedes B. Only prefixes 8/16/32 are retained by the candidates;
  the four final unused modes are offline diagnostic overhead. Axes and mode
  ordering are in the experiment source. Runtime budgets count retained data.
- `decision.json`, `costs.json`, `fit-convergence.json`: storage/fetch accounting,
  measured CPU preparation/verification and kernel-order controls. GPU fetch
  counts describe a conservative layout; they are not measured frame timings.
- `tiny-priority-*`: initial refinement of handoff sharp rows 9/15/18, in that
  order, completed before fitting. These three inputs retain their own numbering.
- `candidate-sanity.txt`, `reference-self-check.txt`, `native-release.txt`:
  independent math checks and retained Release native regression markers.
- `manifest.json`, `capture-provenance.json`: frozen source/input hashes and
  original numeric capture conditions. `history: "None"` records that historical
  reference result files were not used in the fresh oracle run.

This directory is deliberately numerical. It contains no candidate screenshot,
accepted runtime asset, package, motion acceptance or full-frame timing evidence.
The visual gate remains open.
