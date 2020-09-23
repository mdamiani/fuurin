# Architecture decision record


## Title

Sequence number assignment for data.


## Context

Whenever a worker stores a new data, a sequence number must be assigned.
This is needed in order to prevent the forward and delivery of the same data more than once.
Reception of same data might happen at worker level, just after the synchronization with broker.
Such event can happen at broker level too, in case of communication with other peer brokers.


### Actual state

Currenlty there are no sequence numbers.
Any update of data is always accepted by broker and delivered to its network.


### Possible solutions

1. Sequence number assignment at broker:
    - Broker shall mark data with its sequence number, after increasing it, whenever it is received.
    - Broker shall deliver data to its network with its own mark.
    - Broker can have a single sequence number, or a different one for each worker, or for each topic.
    - Worker may send any new data without any sequence number.
    - Worker shall synchronize with broker's latest sequence number.
    - Worker shall discard any data delivered with a preivous sequence number.
    - Broker shall re-send its data to other brokers, eventually.
    - Broker shall re-mark data received from other brokers, in order to deliver it to its network.
    - Broker shall keep a list of previous brokers marks, in order to avoid forwarding already seen data.
    - Worker shall send data with its own sequence number, in case of multiple broker communication channels.
    - Broker shall keep worker's sequence number, in case same worker's data is sent to multiple brokers.


2. Sequence number assignment at worker:
    - Worker shall mark data before sending it to any broker.
    - Worker shall store latest sequence number for each any other worker.
    - Worker shall retrieve workers' sequence numbers at synchronization time with broker.
    - Worker shall hold the maximum value between local sequence numbers and broker's ones.
    - Worker shall discard delivered data in case sequence number is old, for any specific worker.
    - Broker shall get the latest worker's sequence number, at synchronization time.
    - Broker shall discard any update which is marked with an old worker's sequence number.
    - Broker shall forward data to other workers without adding marks.
    - Broker shall stop forwarding data to other workers if sequence number for worker is older than its local copy's.
    - Worker shall synchronize with broker, before submitting updates.
    - Broker may discard any early updates from worker, before synchronization.


3. Sequence number assignment at worker, without sequence number synchronization.
    - As in solution 2, but:
        - Broker shall not get the latest worker's sequence number, at synchronization time.
        - Worker might not synchronize with broker, before submitting updates.
    - Broker shall initialize sequence number to 0, for any new worker.
    - Worker shall either:
        - Use a random uuid.
        - Or provide a way such that its sequence number never decreases, also in case of restarts.



## Decision

Solution 3 seems more robust and simple.
Attention must be paid at configuration level.
Every client will have to either use a random uuid or
initialize its sequence number with last used value.


### Comments

#### Pros

1. Sequence number assignment at broker:
    - Worker can send updates even without any previous data synchronization with broker.
    - Broker will accept any update, especially in case of single communication channel.
    - Worker which restarts with the same uuid won't have any side effects caused by
      its previous instance.

2. Sequence number assignment at worker:
    - Detecting duplicates is easy because marks are assigned by workers.
    - Multiple communication channel to a single broker is supported.
    - Single communication channel to multiple brokers is supported.
    - Broker is simpler.
    - Worker can be customized to prevent some limitations of this solution,
      e.g. saving last sequence number on persistent memory and
      reusing it in case of restart.
    - Worker can submit the same update to multiple brokers.

3. Sequence number assignment at worker, without sequence number synchronization.
    - Broker is simpler, no need to get worker's sequence number.
    - Worker doesn't need to synchronize before sending updates.
    - Worker can send updates to multiple brokers or channels.


#### Cons

1. Sequence number assignment at broker:
    - Worker must manage a sequence number too, in case data is sent twice or more to either
      the same broker (multiple communication channels) or to a different one (to avoid
      forwarding the same message multiple times).
    - Detecting duplicates is difficult since data is marked at broker level.
    - Need of additional worker sequence number to detect duplicates.
    - Brokers shall manage a list of marks for every data.
    - Broker logic is hardly customized.

2. Sequence number assignment at worker:
    - Worker needs to wait for synchronization, before sending any first update.
    - Worker which switched to another broker might experience some discarded data,
      since updates might have an old sequence number.
    - Worker needs to hold sequence numbers of every other worker.
    - Worker using the same uuid and with sequence numbers always starting from 0,
      will have its messages discarded by broker, before the sequence number
      gets updated to the latest value.
    - Worker synchronizes to just one single broker, so in case of multiple connections,
      any other broker might discard worker's data.

3. Sequence number assignment at worker, without sequence number synchronization.
    - It's up to the worker's client to guarantee updates have always increasing sequence numbers.
    - If worker's uuid is not random and sequence number gets backwards, updates will be discarded.


## Status

Accepted


## Consequences

Worker and Broker must manage the sequence number field of every topic, when submitted and delivered.

