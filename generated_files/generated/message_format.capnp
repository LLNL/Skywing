# Generated ID
@0xa7b5195f17d5287f;

# Set a namespace
using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("cpnpro");

struct PublishValue {
  union {
    # Cap'n Proto FORCES camelCase to be used for these...
    d     @0  : Float64;
    rD    @1  : List(Float64);
    f     @2  : Float32;
    rF    @3  : List(Float32);
    i8    @4  : Int8;
    rI8   @5  : List(Int8);
    i16   @6  : Int16;
    rI16  @7  : List(Int16);
    i32   @8  : Int32;
    rI32  @9  : List(Int32);
    i64   @10 : Int64;
    rI64  @11 : List(Int64);
    u8    @12 : UInt8;
    rU8   @13 : List(UInt8);
    u16   @14 : UInt16;
    rU16  @15 : List(UInt16);
    u32   @16 : UInt32;
    rU32  @17 : List(UInt32);
    u64   @18 : UInt64;
    rU64  @19 : List(UInt64);
    str   @20 : Text;
    rStr  @21 : List(Text);
    bytes @22 : Data;
    bool  @23 : Bool;
    rBool @24 : List(Bool);
  }
}

struct PublishData {
  value   @0 : List(PublishValue);
  version @1 : UInt32;
  tagID   @2 : Text;
}

struct Greeting {
  from      @0 : Text;
  neighbors @1 : List(Text);
  port      @2 : UInt16;
}

struct NewNeighbor {
  neighborID @0 : Text;
}

struct RemoveNeighbor {
  neighborID @0 : Text;
}

# For each tag, a list of machines addresses known to publish on that tag
# Additionally, a list of tags that are produced by the machine that sent the message
struct ReportPublishers {
  tags                @0 : List(Text);
  addresses           @1 : List(List(Text));
  machines            @2 : List(List(Text));
  locallyProducedTags @3 : List(Text);
}

struct GetPublishers {
  tags             @0 : List(Text);
  publishersNeeded @1 : List(UInt8);
  ignoreCache      @2 : Bool;
}

struct JoinReduceGroup {
  reduceTag   @0 : Text;
  tagProduced @1 : Text;
}

struct SubmitReduceValue {
  reduceTag  @0 : Text;
  data       @1 : PublishData;
}

struct ReportReduceDisconnection {
  reduceTag         @0 : Text;
  initiatingMachine @1 : Text;
  id                @2 : UInt64;
}

struct SubscriptionNotice {
  tags          @0 : List(Text);
  isUnsubscribe @1 : Bool;
}

struct StatusMessage {
  union {
    greeting                  @0  : Greeting;
    goodbye                   @1  : Void;
    newNeighbor               @2  : NewNeighbor;
    removeNeighbor            @3  : RemoveNeighbor;
    heartbeat                 @4  : Void;
    reportPublishers          @5  : ReportPublishers;
    getPublishers             @6  : GetPublishers;
    joinReduceGroup           @7  : JoinReduceGroup;
    submitReduceValue         @8  : SubmitReduceValue;
    reportReduceDisconnection @9  : ReportReduceDisconnection;
    publishData               @10 : PublishData;
    subscriptionNotice        @11 : SubscriptionNotice;
  }
}
