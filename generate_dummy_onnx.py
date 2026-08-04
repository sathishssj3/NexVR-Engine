import onnx
from onnx import helper
from onnx import TensorProto

import onnx
from onnx import helper
from onnx import TensorProto

def generate_dummy(name, input_shape):
    X = helper.make_tensor_value_info('input', TensorProto.FLOAT, input_shape)
    Y = helper.make_tensor_value_info('output', TensorProto.FLOAT, input_shape)

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
    onnx.save(model, f'models/{name}.onnx')

generate_dummy('dummy', [1, 4, 256, 256])
generate_dummy('dummy_ui', [1, 3, 640, 640])
