# Usage
First use Cov debloating tool to debloat the source code. Do not enable DCE.

Then use the following command to augment the debloated source code with the original source code.

If needed, perform DCE after augmentation.

```bash
cd /workspace/main/debloating_analysis_tools

# get debloated lines
python3 scripts/get_debloated_lines.py debloated.c original.c -o debloated-lines.txt

# augment debloated source code
build/bin/cov_augment --debloated-lines="debloated-lines.txt" --debloated-src="debloated.c" "original.c" --
```
