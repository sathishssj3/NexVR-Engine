import onnx
from onnx import helper
from onnx import TensorProto

# Create a very simple ONNX model (Identity) that takes [1, 3, 256, 256] and outputs it
X = helper.make_tensor_value_info('input', TensorProto.FLOAT, [1, 3, 256, 256])
Y = helper.make_tensor_value_info('output', TensorProto.FLOAT, [1, 3, 256, 256])

node = helper.make_node(
    'Identity',
    inputs=['input'],
    outputs=['output']
)

graph = helper.make_graph(
    [node],
    'dummy_graph',
    [X],
    [Y]
)

model = helper.make_model(graph, producer_name='dummy_model', ir_version=9)
del model.opset_import[:]
opset = model.opset_import.add()
opset.domain = ''
opset.version = 13
onnx.save(model, 'models/dummy.onnx')
