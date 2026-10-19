extends SceneTree
# Counts the seeds from 0 to 199 the core refuses to generate a region from, and lists them.
func _init() -> void:
	var refused := []
	for seed in 200:
		if WorldAdapter.region_refusal(seed) != -1:
			refused.append(seed)
	print("refusals: %d of 200 seeds refused: %s" % [refused.size(), refused])
	quit()
