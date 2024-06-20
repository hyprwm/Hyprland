#include "Buffer.hpp"

void IHLBuffer::sendRelease() {
    resource->sendRelease();
}
