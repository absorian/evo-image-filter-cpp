./build/evo_image_filter_cpp \
  -o output.png \
  -j 24 \
  --shapes-per-save 10 \
  --threshold 6000 \
  --swarm 500 \
  --survived 125 \
  --generations 5 \
  --children 4 \
  test_images/royal_boredom.jpg \
  test_shapes/icons
