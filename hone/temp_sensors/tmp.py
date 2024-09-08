#
# author: Jonas Behr 
# date: Sat Aug 24 15:37:02 2024
######################################

import plotly.express as px
import pandas as pd
import numpy as np
from numpy.polynomial import Chebyshev
from data_new_op_amp import tmp
from numpy.polynomial.polynomial import polyfit
import plotly.graph_objects as go

def calc_temp(z1, z2, split_val, val):
    if val >= split_val:
        z = z2
    else:
        z=z1
    return sum([val**p * z[p] for p in range(len(z))])


def picewise_polynomial_fit(df, col="tmp1", deg=3):
    vals = sorted(list(set(df[col])))
    best_loss = 1e10
    for split_val in vals[10:-10]:
        idx = df[col] <= split_val
        z1 = polyfit(df[idx][col], df[idx]["target"], deg=deg)
        idx = df[col] >= split_val
        z2 = polyfit(df[idx][col], df[idx]["target"], deg=deg)
        df["pred"] = df[col].apply(lambda x: calc_temp(z1, z2, split_val, x))
        loss = sum((df["pred"] - df["target"])**2) / len(df)
        #print(f"split_val: {split_val} loss: {loss}")
        if loss < best_loss:
            best_z1 = z1
            best_z2 = z2
            best_split = split_val
            best_loss = loss
    return {"z1": best_z1, "z2" : best_z2, "split_val": best_split}


df = []
for key in tmp.keys():
    df1 = pd.DataFrame({"tmp1": tmp[key]})
    df1["target"] = key
    df.append(df1)

df = pd.concat(df)

model = picewise_polynomial_fit(df)

#tmp_calcs = {}
#for col in ["tmp1", "tmp2"]:
#    tmp_calcs[col] = {
#            "median":{}, 
#            "std":{}, 
#            }
#    z = polyfit(df[col], df["target"], deg=5)
#    print(f"{col}: {z}")
#    tmp_calcs[col]["poly_fit"] = z
#    for key in tmp.keys():
#        tmp_calcs[col]["median"][key] = np.median(df[df['target']==key][col])
#        tmp_calcs[col]["std"][key] = np.std(df[df['target']==key][col])
#        print(f"Temp sensor {col} at temp {key}: mean: {tmp_calcs[col]['median']} std: {tmp_calcs[col]['std']}")

df["pred"] = df["tmp1"].apply(lambda v: calc_temp(**model, val=v))

min_val = min(list(df["tmp1"]))
max_val = max(list(df["tmp1"]))
x_range = np.array(range(int(min_val - 10), int(max_val + 10)))
y_vals = []
for col in ["tmp1"]:
    val = [calc_temp(**model, val=v) for v in x_range]
    y_vals.append(val)

fig = px.line(x=x_range, y=y_vals)
for col in ["tmp1"]:
    fig.add_trace(
        go.Scatter(x=df[col], y=df[f"target"], mode='markers', name=f"data_{col}")
    )

fig.show()



fig = px.scatter(df, x="target", y="pred")
x = np.array([min(df["target"]), max(df["target"])])
fig.add_trace(
    go.Scatter(x=x, y=x, name="slope", line_shape='linear')
)
fig.show()
print(model)
