local timer = 0.0

function init()

end

function update(entity, dt)
    
    timer = timer + dt
    entity.sy = 1.0 + math.sin(timer*0.4)
    entity.vy = 0.5 + math.cos(timer*0.4)

end