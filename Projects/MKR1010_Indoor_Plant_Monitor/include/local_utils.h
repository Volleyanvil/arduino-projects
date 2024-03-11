
typedef struct moisture_sensor {
  uint8_t pin;
  short val;
  long sum;
  short base;
  short cap;
  // bool capacitive;
  char id[10];
  char val_id[20];
} mst_sen;

typedef mst_sen mst_sen_arr[];
