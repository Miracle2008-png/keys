// Real Objective-C: interfaces, properties, message sends, blocks.
#import <Foundation/Foundation.h>

@protocol Describable <NSObject>
- (NSString *)describe;
@end

@interface Invoice : NSObject <Describable>

@property (nonatomic, assign) NSInteger identifier;
@property (nonatomic, copy, nullable) NSString *customer;
@property (nonatomic, assign) double amount;

- (instancetype)initWithIdentifier:(NSInteger)identifier amount:(double)amount;

@end

@implementation Invoice

- (instancetype)initWithIdentifier:(NSInteger)identifier amount:(double)amount {
    self = [super init];
    if (self) {
        _identifier = identifier;
        _amount = amount;
    }
    return self;
}

- (NSString *)describe {
    if (self.amount > 10000.0) {
        return @"large";
    }
    return self.amount > 0.0 ? @"standard" : @"empty";
}

+ (NSArray<Invoice *> *)overdue:(NSArray<Invoice *> *)invoices {
    return [invoices filteredArrayUsingPredicate:
            [NSPredicate predicateWithBlock:^BOOL(Invoice *inv, NSDictionary *bindings) {
        return inv.amount > 0;
    }]];
}

@end
