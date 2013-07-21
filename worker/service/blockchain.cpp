#include "blockchain.hpp"

#include "../node_impl.hpp"
#include "../echo.hpp"

using namespace bc;
using std::placeholders::_1;
using std::placeholders::_2;

void history_fetched(const std::error_code& ec,
    const blockchain::history_list& history,
    const incoming_message& request, zmq_socket_ptr socket);

void blockchain_fetch_history(node_impl& node,
    const incoming_message& request, zmq_socket_ptr socket)
{
    const data_chunk& data = request.data();
    if (data.size() != 1 + short_hash_size)
    {
        log_error(LOG_WORKER)
            << "Incorrect data size for blockchain.fetch_history";
        return;
    }
    uint8_t version_byte;
    short_hash hash;
    auto deserial = make_deserializer(data.begin(), data.end());
    version_byte = deserial.read_byte();
    hash = deserial.read_short_hash();
    BITCOIN_ASSERT(deserial.iterator() == data.end());
    payment_address payaddr;
    if (!payaddr.set_raw(version_byte, hash))
    {
        log_error(LOG_WORKER)
            << "Problem setting address in blockchain.fetch_history";
        return;
    }
    log_debug(LOG_WORKER) << "blockchain.fetch_history("
        << payaddr.encoded() << ")";
    node.blockchain().fetch_history(payaddr,
        std::bind(history_fetched, _1, _2, request, socket));
}
void history_fetched(const std::error_code& ec,
    const blockchain::history_list& history,
    const incoming_message& request, zmq_socket_ptr socket)
{
    size_t row_size = 36 + 4 + 8 + 36 + 4;
    data_chunk result(row_size * history.size());
    auto serial = make_serializer(result.begin());
    for (const blockchain::history_row row: history)
    {
        serial.write_hash(row.output.hash);
        serial.write_4_bytes(row.output.index);
        serial.write_4_bytes(row.output_height);
        serial.write_8_bytes(row.value);
        serial.write_hash(row.spend.hash);
        serial.write_4_bytes(row.spend.index);
        serial.write_4_bytes(row.spend_height);
    }
    BITCOIN_ASSERT(serial.iterator() == result.end());
    outgoing_message response(request, result);
    log_debug(LOG_WORKER)
        << "blockchain.fetch_history() finished. Sending response.";
    response.send(*socket);
}

