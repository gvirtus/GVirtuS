# Protocol v2 asynchronous request engine

Protocol v2 tracks multiple outstanding requests by their 64-bit request IDs.
Queued, CUDA-submitted, completed, failed, and cancelled are distinct states;
submission never implies GPU completion.

Request IDs must be non-zero and strictly increasing for the lifetime of an
engine. Collecting a response does not make its ID reusable. This prevents a
late or duplicated response from being correlated with different work.

Each operation depends on the preceding request in the same virtual stream, so
different streams can become ready concurrently while preserving stream order.
A device-synchronization request depends on every non-terminal request already
known, and later work depends on that barrier. Stream synchronization uses the
same per-stream dependency chain as other stream operations.

The engine enforces a configured maximum number of retained requests. This
includes terminal responses that have not yet been collected, providing
backpressure for both execution and response queues. Responses are retrieved by
request ID and cannot be taken before a terminal state.

Completion removes satisfied dependencies. Failure or cancellation recursively
cancels dependent work instead of executing it after a broken prerequisite.
Session teardown cancels every non-terminal request and clears ordering tails.

This is a transport- and CUDA-independent state machine. A later dispatcher
will connect its ready set to CUDA submission and will call completion methods
only after actual stream/event completion.
