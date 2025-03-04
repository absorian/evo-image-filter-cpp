./build/evo_image_filter_cpp \
  -o output.png \
  -j 20 \
  --sortout-percent 100 \
  --shapes-save 5 \
  --shapes 2000 \
  --init-swarm 800 \
  --survived-count 150 \
  --generations 5 \
  --shape-resize 100 \
  test_images/monty1.jpg \
  test_shapes/drops
